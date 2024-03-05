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

#include <cstdio>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <cstring>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

namespace searchserver {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::bind_and_listen(int *listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // NOTE: You only have to support IPv6, you do not have to support IPv4

  // Set hints for getaddrinfo to create an IPv6 socket
  struct addrinfo hints, *result;
  // Zero out the hints data structure using memset
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET6; // IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP
  hints.ai_flags = AI_PASSIVE; // Use wildcard IP address
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // Get the address information for the local host (NULL to indicate no filter on port)
  int status = getaddrinfo(NULL, std::to_string(port_).c_str(), &hints, &result);
  if (status != 0) {
    std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
    return false;
  }

  // Create the socket
  int sock_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sock_fd == -1) {
    std::cerr << "socket error: " << strerror(errno) << std::endl;
    freeaddrinfo(result);
    return false;
  }

  // Bind the socket to the local address
  if (bind(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
    std::cerr << "bind error: " << strerror(errno) << std::endl;
    close(sock_fd);
    freeaddrinfo(result);
    return false;
  }

  // Listen for incoming connections
  if (listen(sock_fd, SOMAXCONN) == -1) {
    std::cerr << "listen error: " << strerror(errno) << std::endl;
    close(sock_fd);
    freeaddrinfo(result);
    return false;
  }

  // Set the output parameter and the member variable
  *listen_fd = sock_fd;
  listen_sock_fd_ = sock_fd;

  // Free the address info
  freeaddrinfo(result);

  return true;
}

bool ServerSocket::accept_client(int *accepted_fd,
                          std::string *client_addr,
                          uint16_t *client_port,
                          std::string *client_dns_name,
                          std::string *server_addr,
                          std::string *server_dns_name) const {

  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.
  struct sockaddr_in6 client_addr_in;
  socklen_t addrlen = sizeof(client_addr_in);
  int newsockfd;
 
  while(1) {
    newsockfd = accept(listen_sock_fd_, reinterpret_cast<struct sockaddr *>(&client_addr_in), &addrlen);
    if (newsockfd == -1) {
        if(errno == EINTR) {
            continue;
        }
        return false;
    }
    break;
  }

  // Get client address info
  char client_ip[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &(client_addr_in.sin6_addr), client_ip, INET6_ADDRSTRLEN);
  *client_addr = std::string(client_ip);
  *client_port = ntohs(client_addr_in.sin6_port);
  char hostname[1024];

  // Get client DNS name, if getname info fails, use IPV6 address as dns_name
  if (getnameinfo((const struct sockaddr *) &client_addr_in, addrlen, hostname, 1024, nullptr, 0, 0) != 0){
      *client_dns_name = *client_addr;
  }
  std::string p(hostname);
  *client_dns_name = p;

  // Get server address info
  struct sockaddr_in6 server_addr_in;
  socklen_t server_addrlen = sizeof(server_addr_in);
  getsockname(newsockfd, (struct sockaddr *) &server_addr_in, &server_addrlen);
  char server_ip[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &(server_addr_in.sin6_addr), server_ip, INET6_ADDRSTRLEN);
  *server_addr = std::string(server_ip);
  char hname[1024];

  // Get server DNS name
  if (getnameinfo((const struct sockaddr *) &server_addr_in, server_addrlen, hname, 1024, nullptr, 0, 0) != 0){
      *server_dns_name = *server_addr;
  }
  std::string h(hname);
  *server_dns_name = h;

  // Set output parameter
  *accepted_fd = newsockfd;

  return true;
}

}  // namespace searchserver
