#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "macros.h"

#include "logging.h"

namespace Common {
  struct SocketCfg {
    std::string ip_;
    std::string iface_;
    int port_ = -1;
    bool is_udp_ = false;
    bool is_listening_ = false;
    bool needs_so_timestamp_ =  false;

    auto toString() const {
      std::stringstream ss;
      ss << "SocketCfg[ip:" << ip_
      << " iface:" << iface_
      << " port:" << port_
      << " is_udp:" << is_udp_
      << " is_listening:" << is_listening_
      << " needs_SO_timestamp:" << needs_so_timestamp_
      << "]";

      return ss.str();
    }
  };

  /// Represents the maximum number of pending / unaccepted TCP connections.
  constexpr int MaxTCPServerBacklog = 1024;

  // Get network interface IP number from name (for example, from interface name "eth0", returning the ip "123.123.123.123")
  inline auto getIfaceIP(const std::string &iface) -> std::string {
    char buf[NI_MAXHOST] = {'\0'};
    ifaddrs *ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != -1) { 
      // then a list of interface addresses has been assigned to ifaddr.
      for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        // Do we have an address? Does the belong to the AF_INET family? Does the address name match the name we are searching?
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
          // if so, obtain the ip address from the given ifaddrs item and assign it to buf
          int s = getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
          ASSERT(!s, "getnameinfo() failed. error:" + std::string(gai_strerror(s)) + "errno:" + strerror(errno));
          break;
        }
      }
      // The data returned by getifaddrs() is dynamically allocated and 
      //  should be freed using freeifaddrs() when no longer needed.
      freeifaddrs(ifaddr);
    }

    return buf; // return IP name address for name
  }


  inline auto setNonBlocking(int fd) -> bool {
    // Perform F_GETFL operation on socket file descriptor "fd". 
    // In this case, F_GETFL will open the file description for the descriptor and
    // get its associated status flags,
    const auto flags = fcntl(fd, F_GETFL, 0);
    // If one of those flags includes O_NONBLOCK that means the socket is non-blocking
    if (flags & O_NONBLOCK)
      return true;
    // Otherwise, we have a blocking socket, so we add a non-blocking flag on it
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);
  }

  /// Disable Nagle's algorithm and associated delays.
  inline auto disableNagle(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
  }

  /// Allow software receive timestamps on incoming packets.
  inline auto setSOTimestamp(int fd) -> bool {
    int one = 1;
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one)) != -1);
  }

  /// Add / Join membership / subscription to the multicast stream specified and on the interface specified.
  inline auto join(int fd, const std::string &ip) -> bool {
    const ip_mreq mreq{
      {inet_addr(ip.c_str())}, /* contains the address of the multicast group the application wants to join or leave*/
      {htonl(INADDR_ANY)} /*the address of the local interface with which the system should join the multicast group; if it is equal to INADDR_ANY, an appropiate interface is chosen by the system */
    };
    return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
  }

  /// Create a TCP / UDP socket using the input configuration, to either connect to or listen for data on or listen for connections on the specified interface and IP:port information.
  [[nodiscard]] inline auto createSocket(Logger &logger, const SocketCfg& socket_cfg) -> int {
    std::string time_str;
    // Do we have an IP? If not, get network interface IP from name
    const auto ip = socket_cfg.ip_.empty() ? getIfaceIP(socket_cfg.iface_) : socket_cfg.ip_;
    logger.log("%:% %() % configuration:%\n", __FILE__, __LINE__, __FUNCTION__,
               Common::getCurrentTimeStr(&time_str), socket_cfg.toString());

    // Collect a bunch of flags into "hints", 
    //  to use when selecting candidate socket addresses
    const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);

    const addrinfo hints{input_flags, 
                         AF_INET, 
                         socket_cfg.is_udp_ ? SOCK_DGRAM : SOCK_STREAM, // allows for UDP/TCP protocols
                         socket_cfg.is_udp_ ? IPPROTO_UDP : IPPROTO_TCP, // set to UDP/TCP protocol
                         0, 
                         0, 
                         nullptr, 
                         nullptr};

    // Convert host (ip) and service (port) into socket address (stored at "result").
    //  The "hints" argument points to an "addrinfo" structure that specifies
    //  criteria for selecting the socket address structures returned in
    //  the list pointed to by "result". 
    addrinfo *result = nullptr;

    const auto rc = getaddrinfo(ip.c_str(), std::to_string(socket_cfg.port_).c_str(), &hints, &result);
    ASSERT(!rc, "getaddrinfo() failed. error:" + std::string(gai_strerror(rc)) + "errno:" + strerror(errno));

    // Loop over all candidate socket addresses
    int socket_fd = -1, one = 1;
    for (addrinfo *rp = result; rp; rp = rp->ai_next) {
      // Can we build a socket using the family, type and protocol for this item?
      ASSERT((socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1, "socket() failed. errno:" + std::string(strerror(errno)));
      // Yes; then start working with the newly build socket
      // Set it to non-blocking
      ASSERT(setNonBlocking(socket_fd), "setNonBlocking() failed. errno:" + std::string(strerror(errno)));

      if (!socket_cfg.is_udp_) { 
        // disable Nagle for TCP sockets.
        ASSERT(disableNagle(socket_fd), "disableNagle() failed. errno:" + std::string(strerror(errno)));
      }

      // (A: connect)  if it is not a listening socket, we connect the socket to the target address 
      if (!socket_cfg.is_listening_) { // establish connection to specified address.
        ASSERT(connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != 1, "connect() failed. errno:" + std::string(strerror(errno)));
      }

      //  (B: bind & listen)  if we want to create a socket that listens for incoming connections, we...
      // 1. Set the correct parameters and bind the socket to a specific 
      //    address that the client will try to connect to. 
      if (socket_cfg.is_listening_) { 
        ASSERT(setsockopt(socket_fd, 
                          SOL_SOCKET, /* flag category/level */ 
                          SO_REUSEADDR, /* flag: allow re-using the address in the call to bind() -
                          When an incoming connection arrives, we want the listening socket to create 
                          a new socket and establish the connection there so that the listening socket
                          can resume listening on the same port */
                          reinterpret_cast<const char *>(&one), sizeof(one)) == 0, "setsockopt() SO_REUSEADDR failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.is_listening_) {
        // bind to the specified port number.
        const sockaddr_in addr{AF_INET, htons(socket_cfg.port_), {htonl(INADDR_ANY)}, {}};
        ASSERT(bind(socket_fd, socket_cfg.is_udp_ ? reinterpret_cast<const struct sockaddr *>(&addr) : rp->ai_addr, sizeof(addr)) == 0, "bind() failed. errno:%" + std::string(strerror(errno)));
      }

      // 2. We also need to call the listen() routine for such a socket configuration. 
      if (!socket_cfg.is_udp_ && socket_cfg.is_listening_) { // listen for incoming TCP connections.
        ASSERT(listen(socket_fd, MaxTCPServerBacklog) == 0, "listen() failed. errno:" + std::string(strerror(errno)));
      }

      if (socket_cfg.needs_so_timestamp_) { // enable software receive timestamps.
        ASSERT(setSOTimestamp(socket_fd), "setSOTimestamp() failed. errno:" + std::string(strerror(errno)));
      }
    }

    return socket_fd; // -1 means failure.
  }
}
