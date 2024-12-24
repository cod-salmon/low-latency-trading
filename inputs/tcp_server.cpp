#include "tcp_server.h"

namespace Common {
  /// Add and remove socket file descriptors to and from the EPOLL list.
  auto TCPServer::addToEpollList(TCPSocket *socket) {
    epoll_event ev{EPOLLET | EPOLLIN, {reinterpret_cast<void *>(socket)}};
    
    return !epoll_ctl(epoll_fd_,          // file descriptor referring to epoll instance
                      EPOLL_CTL_ADD,      // in this case actually "added" (see below)
                      socket->socket_fd_, // file descriptor to be added/removed/updated in the interest list of the referred epoll instance
                      &ev                 /* (In this case):
                                              (1) Requests edge-triggered notification for the associated file descriptor.
                                              (2) The associated file is available for read(2) operations.
                                              (3) Type of user data variable */
                      );                  // If successful, returns 0         
  }

  /// Start listening for connections on the provided interface and port.
  auto TCPServer::listen(const std::string &iface, int port) -> void {
    epoll_fd_ = epoll_create(1);
    
    ASSERT(epoll_fd_ >= 0, "epoll_create() failed error:" + std::string(std::strerror(errno)));

    ASSERT(listener_socket_.connect("", iface, port, true) >= 0,
           "Listener socket failed to connect. iface:" + iface + " port:" + std::to_string(port) + " error:" +
           std::string(std::strerror(errno)));

    ASSERT(addToEpollList(&listener_socket_), "epoll_ctl() failed. error:" + std::string(std::strerror(errno)));
  }

  /// Publish outgoing data from the send buffer and read incoming data from the receive buffer.
  auto TCPServer::sendAndRecv() noexcept -> void {
    auto recv = false;

    // Collects data from these sockets
    std::for_each(receive_sockets_.begin(), receive_sockets_.end(), [&recv](auto socket) {
      recv |= socket->sendAndRecv(); // if return, data has been sent
    });

    if (recv) // There were some events and they have all been dispatched, inform listener.
      recv_finished_callback_();

    // Send data to these sockets
    std::for_each(send_sockets_.begin(), send_sockets_.end(), [](auto socket) {
      socket->sendAndRecv();
    });
  }

  /// Check for new connections or dead connections and update containers that track the sockets.
  auto TCPServer::poll() noexcept -> void {
    const int max_events = 1 + send_sockets_.size() + receive_sockets_.size();

    const int n = epoll_wait(epoll_fd_, events_, max_events, 0); // Get events from epoll into events_. 
    // Returns the number of file descriptors ready for the requested I/O operation
    bool have_new_connection = false;
    for (int i = 0; i < n; ++i) {
      const auto &event = events_[i];
      auto socket = reinterpret_cast<TCPSocket *>(event.data.ptr);

      // EPOLLIN flag meaning having data to read from
      if (event.events & EPOLLIN) {
        if (socket == &listener_socket_) { 
          // If the socket is listener_socket, then we have a new connection request
          //  (instead of an I/O request), so we break the loop and add new connection 
          //  (socket) to the epoll interest list.
          logger_.log("%:% %() % EPOLLIN listener_socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                      Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
          have_new_connection = true;
          continue;
        }
        // Gather all sockets which to receive data from
        logger_.log("%:% %() % EPOLLIN socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
        if (std::find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
          receive_sockets_.push_back(socket);
      }
      // Gather all sockets which to send data to
      if (event.events & EPOLLOUT) {
        logger_.log("%:% %() % EPOLLOUT socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
        if (std::find(send_sockets_.begin(), send_sockets_.end(), socket) == send_sockets_.end())
          send_sockets_.push_back(socket);
      }

      if (event.events & (EPOLLERR | EPOLLHUP)) {
        logger_.log("%:% %() % EPOLLERR socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
        if (std::find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
          receive_sockets_.push_back(socket);
      }
    }

    // Accept a new connection, create a TCPSocket for that connection and add it to our containers.
    while (have_new_connection) { /* this is an infinite loop, which will be broken only
      when the accept() function below stops polling file descriptors out of the queue
      of file descriptors pending to be connected to the server. */
      logger_.log("%:% %() % have_new_connection\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_));
      sockaddr_storage addr;
      socklen_t addr_len = sizeof(addr);
      
      int fd = accept(listener_socket_.socket_fd_, reinterpret_cast<sockaddr *>(&addr), &addr_len);
      // The accept() system call extracts the first
      //  connection request on the queue of pending connections for the
      //  listening socket, listener_socket_.socket_fd_, creates a new connected socket, and
      //  returns a new file descriptor referring to that socket.
      if (fd == -1)
        break;

      ASSERT(setNonBlocking(fd) && disableNagle(fd),
             "Failed to set non-blocking or no-delay on socket:" + std::to_string(fd));

      logger_.log("%:% %() % accepted socket:%\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), fd);

      // Create a socket with the fd polled from the fd queue for connections to the listener socket
      // Add socket for monitoring to epoll list
      auto socket = new TCPSocket(logger_);
      socket->socket_fd_ = fd;
      socket->recv_callback_ = recv_callback_;
      ASSERT(addToEpollList(socket), "Unable to add socket. error:" + std::string(std::strerror(errno)));
      // Add socket if it's still not in the "sockets that will receive the listener's data" set 
      if (std::find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end())
        receive_sockets_.push_back(socket);
    }
  }
}
