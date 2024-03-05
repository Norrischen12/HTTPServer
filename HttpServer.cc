/*
 * Copyright ©2023 Travis McGaha.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Pennsylvania
 * CIT 5950 for use solely during Spring Semester 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"


using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

namespace searchserver {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
static const char *kFivegleStr =
  "<html><head><title>5950gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">5</span>"
    "<span style=\"color:red;\">9</span>"
    "<span style=\"color:gold;\">5</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// static
const int HttpServer::kNumThreads = 100;

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &base_dir,
                            WordIndex *indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                 WordIndex *index);


///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.bind_and_listen(&listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  cout << "  accepting connections..." << endl << endl;
  ThreadPool tp(kNumThreads);
  while (1) {
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn);
    hst->base_dir = static_file_dir_path_;
    hst->index = index_;
    if (!socket_.accept_client(&hst->client_fd,
                    &hst->c_addr,
                    &hst->c_port,
                    &hst->c_dns,
                    &hst->s_addr,
                    &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.
      // (Will happen when kill command is used to shut down the server.)
      break;
    }
    // The accept succeeded; dispatch it.
    tp.dispatch(hst);
  }
  return true;
}

static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new
  // client's information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask *>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, write the response.

  // Use the HttpConnection class to read and process the next
  // request from our current client, then write out our response.  If
  // the client sends a "Connection: close\r\n" header, then shut down
  // the connection -- we're done.
  //
  // Hint: the client can make multiple requests on our single connection,
  // so we should keep the connection open between requests rather than
  // creating/destroying the same connection repeatedly.

  // TODO: Implement
  HttpConnection connection(hst->client_fd);
  bool done = false;
  while (!done) {
    HttpRequest request;
    if (!connection.next_request(&request)) {
      done = true;
      continue;
    }
    HttpResponse response = ProcessRequest(request, hst->base_dir, hst->index);
    if (!connection.write_response(response)) {
      done = true;
      continue;
    }
    if (request.GetHeaderValue("Connection") == "close") {
      done = true;
      continue;
    }
  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &base_dir,
                            WordIndex *index) {
  // Is the user asking for a static file?
  if (req.uri().substr(0, 8) == "/static/") {
    return ProcessFileRequest(req.uri(), base_dir);
  }
  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), index);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  //  - use the URLParser class to figure out what filename
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  string file_name = ".";
  URLParser parser;
  parser.parse(uri);
  file_name = file_name + parser.path().substr(7);

  // If path is not safe, return an HTTP 404 error
  if(!is_path_safe(base_dir, file_name)){
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                    + escape_html(file_name)
                    + "\"</body></html>\n");
    return ret;
  }

  //  - use the FileReader class to read the file into memory
  FileReader reader(file_name);
  string file_contents;
  // If couldn't find the file, return an HTTP 404 error
  if (!reader.read_file(&file_contents)) {
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                     + escape_html(file_name)
                     + "\"</body></html>\n");
    return ret;
  }
  ret.AppendToBody(file_contents);

  //  - depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  string extension;
  size_t dot_pos = file_name.find_last_of(".");
  if (dot_pos == string::npos) {
    extension = "";
  } else {
    extension = file_name.substr(dot_pos + 1);
  }

  if (extension == "html" || extension == "htm") {
    ret.set_content_type("text/html");
  } else if (extension == "jpeg" || extension == "jpg") {
    ret.set_content_type("image/jpeg");
  } else if (extension == "png") {
    ret.set_content_type("image/png");
  } else if (extension == "txt") {
    ret.set_content_type("text/txt");
  } else if (extension == "js") {
    ret.set_content_type("application/javascript");
  } else if (extension == "css") {
    ret.set_content_type("text/css");
  } else if (extension == "xml") {
    ret.set_content_type("text/xml");
  } else if (extension == "gif") {
    ret.set_content_type("image/gif");
  } else {
    ret.set_content_type("application/octet-stream");
  }
  
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");

  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                 WordIndex *index) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/httpd server.
  // A couple of notes:
  //
  //  - no matter what, you need to present the 5950gle logo and the
  //    search box/button
  //
  //  - if the user sent in a search query, you also
  //    need to display the search results. You can run the solution binaries to see how these should look
  //
  //  - you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  //  - Use the specified index to generate the query results

  // TODO: implement
  //  Get the query string from the URL parser
  URLParser parser;
  parser.parse(uri);
  string query = parser.args()["terms"];
  boost::algorithm::to_lower(query);

  // Perform the search and add the results to the HTML
  vector<string> query_list;
  boost::split(query_list, query, boost::is_any_of(", "), boost::token_compress_on);
  vector<Result> results = index->lookup_query(query_list);
  
  ret.AppendToBody(string(kFivegleStr) + "<p>");
  if (uri.find("?") != string::npos) {
    if (results.size() == 0) {
      ret.AppendToBody("No");
    } else {
      ret.AppendToBody(std::to_string(results.size()));
    }
    ret.AppendToBody(" search results for: <b>");
    ret.AppendToBody(escape_html(query) + "</b></p>");
    ret.AppendToBody("<p> </p>");
  }
  ret.AppendToBody("<ul>");
  for (auto &result : results) {
    ret.AppendToBody("<li><a href=\"/static/");
    ret.AppendToBody(result.doc_name);
    ret.AppendToBody("\">");
    ret.AppendToBody(result.doc_name);
    ret.AppendToBody("</a> [");
    ret.AppendToBody(std::to_string(result.rank));
    ret.AppendToBody("] <br> </li>");
  }
  ret.AppendToBody("</ul></body></html>");

  // Set the response HTML
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_content_type("text/html");
  ret.set_message("OK");

  return ret;
}
}  // namespace searchserver
