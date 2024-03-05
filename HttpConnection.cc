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
#include <iostream>
#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;
using namespace std;

namespace searchserver {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

// TODO: implement
bool HttpConnection::next_request(HttpRequest *request) {
  int pos = 0;
  string header;

  // Use "wrapped_read" to read data into the buffer_
  // instance variable.  Keep reading data until either the
  // connection drops or you see a "\r\n\r\n" that demarcates
  // the end of the request header.
  while (true) {
    if (pos < 0 || buffer_.find(kHeaderEnd) != string::npos) break;
    string word;
    pos = wrapped_read(fd_, &word);
    buffer_ += word;
  }

  // Very tricky part:  clients can send back-to-back requests
  // on the same socket.  So, you need to preserve everything
  // after the "\r\n\r\n" in buffer_ for the next time the
  // caller invokes next_request()!
  size_t header_end_pos = buffer_.find(kHeaderEnd);
  if (header_end_pos == buffer_.size() - kHeaderEndLen){
    header = buffer_.substr(0, header_end_pos);
    buffer_.clear();
  } else {
    header = buffer_.substr(0, header_end_pos);
    buffer_ = buffer_.substr(header_end_pos + kHeaderEndLen);
  }

  // Once you've seen the request header, use parse_request()
  // to parse the header into the *request argument.
  return parse_request(header, request);
}

bool HttpConnection::write_response(const HttpResponse &response) {
  // Implement so that the response is converted to a string
  // and written out to the socket for this connection
  string response_str = response.GenerateResponseString();

  // Write the response string to the socket
  int bytes_written = wrapped_write(fd_, response_str.c_str());
  if (bytes_written == -1) {
    return false;
  }
  return true;
}

bool HttpConnection::parse_request(const string &request, HttpRequest* out) {
  HttpRequest req("/");  // by default, get "/".

  // Split the request into lines.  Extract the URI from the first line
  // and store it in req.URI.  For each additional line beyond the
  // first, extract out the header name and value and store them in
  // req.headers_ (i.e., HttpRequest::AddHeader).  You should look
  // at HttpRequest.h for details about the HTTP header format that
  // you need to parse.
  //
  // You'll probably want to look up boost functions for (a) splitting
  // a string into lines on a "\r\n" delimiter, (b) trimming
  // whitespace from the end of a string, and (c) converting a string
  // to lowercase.
  //
  // If a request is malfrormed, return false, otherwise true and 
  // the parsed request is retrned via *out
  
  // TODO: implement
  // Split the request into lines on a "\r\n" delimiter
  vector<string> lines;
  boost::split(lines, request, boost::is_any_of("\r\n"), boost::token_compress_on);

  if (lines.empty()) {
    return false;
  }
  vector<string> tokens;
  boost::split(tokens, lines[0], boost::is_any_of(" "), boost::token_compress_on);
  if (tokens.size() != 3) {
    return false;
  }
  req.set_uri(tokens[1]);

  // Extract header name and value for each additional line
  for (size_t i = 1; i < lines.size(); i++) {
    const string& line = lines[i];
    size_t pos = line.find(':');
    if (pos == string::npos || pos == string::npos - 1) {
      continue; 
    }
    string name = line.substr(0, pos);
    string value = line.substr(pos + 1);
    boost::trim(name);
    boost::trim(value);
    boost::to_lower(name);
    req.AddHeader(name, value);
  }

  *out = std::move(req);
  return true;
}

}  // namespace searchserver
