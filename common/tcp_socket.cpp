#include "tcp_socket.h"

namespace Common {
  /// Create TCPSocket with provided attributes to either listen-on / connect-to.
  auto TCPSocket::connect(const std::string &ip, const std::string &iface, int port, bool is_listening) -> int {
    // Note that needs_so_timestamp=true for FIFOSequencer.
    const SocketCfg socket_cfg{ip, iface, port, false /*is_udp_*/, is_listening, true /*needs_so_timestamp_*/};
    socket_fd_ = createSocket(logger_, socket_cfg); // if -1, failed to create socket

    socket_attrib_.sin_addr.s_addr = INADDR_ANY; // listen on all available interfaces
    socket_attrib_.sin_port = htons(port);
    socket_attrib_.sin_family = AF_INET; // AF_INET for IPv4.

    return socket_fd_;
  }

  /// Called to publish outgoing data from the buffers as well as check for and callback if data is available in the read buffers.
  auto TCPSocket::sendAndRecv() noexcept -> bool {
    char ctrl[CMSG_SPACE(sizeof(struct timeval))];          // define some control message buffer
    auto cmsg = reinterpret_cast<struct cmsghdr *>(&ctrl);  // use it to instatiate a cmsghdr struct

    // Wrap memory for inbound_data from socket into iov struct
    iovec iov{inbound_data_.data() + next_rcv_valid_index_, // Start of the memory address for incoming data
              TCPBufferSize - next_rcv_valid_index_};       // Size of the memory address for incoming data
    

    // Wrap memory for socket's inbound_data into msghr struct 
    msghdr msg{&socket_attrib_,         // address of socket_attrib_ struct
               sizeof(socket_attrib_),  // size of socket_attrib_ struct
               &iov,                    // where message from socket will be stored
               1,                       // size of struct
               ctrl,                    // ancillary data
               sizeof(ctrl),            // size of ancillary data
               0                        // flags
            };

    // Receive message from socket_fd_'s socket and store it at msg (this socket's inbound data)
    //  - Note: Non-blocking call to read available data.
    const auto read_size = recvmsg(socket_fd_,    // socket to listen data from (this)
                                    &msg,         // msghdr struct to receive the data in (will update "inbound_data_")
                                    MSG_DONTWAIT  // flags: enable non-blocking
                                  );              // Return the number of bytes received, or -1 if an error occurred.

    if (read_size > 0) { // error not ocurred, some bytes received
      next_rcv_valid_index_ += read_size; // update where to receive data next

      // Get how long it took for data to be received
      Nanos kernel_time = 0;
      timeval time_kernel;
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_TIMESTAMP &&
          cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))) {


        memcpy(&time_kernel,          // copy data from "cmsg" to this timeval struct
               CMSG_DATA(cmsg),       // Returns a pointer to the data portion of a cmsghdr (this case "cmsg"). 
               sizeof(time_kernel));  // number of bytes to be copied from "cmsg"
        kernel_time = time_kernel.tv_sec * NANOS_TO_SECS + time_kernel.tv_usec * NANOS_TO_MICROS; // convert timestamp to nanosesconds.
      }

      const auto user_time = getCurrentNanos();

      logger_.log("%:% %() % read socket:% len:% utime:% ktime:% diff:%\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), socket_fd_, next_rcv_valid_index_, user_time, kernel_time, (user_time - kernel_time));

      recv_callback_(this, kernel_time); // function wrapper taking time passed and this socket, with updated inbound data.
      // Should put next_rcv_valid_index_ back to zero and make this socket send a reply confirming the message has been received.
    }

    if (next_send_valid_index_ > 0) { // Has been increased through TCPSocket::send
      // Non-blocking call to send data.
      const auto n = ::send(socket_fd_,                     // socket to send data to
                            outbound_data_.data(),          // from this data buffer 
                            next_send_valid_index_,         // buffer size
                            MSG_DONTWAIT | MSG_NOSIGNAL);   // flags; i.e., non-blocking

      logger_.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_, n);
    }
    next_send_valid_index_ = 0;

    return (read_size > 0);
  }

  // Write "data" into socket's outbound_data_ buffer
  auto TCPSocket::send(const void *data, size_t len) noexcept -> void {
    memcpy(outbound_data_.data() + next_send_valid_index_, 
           data, 
           len);
    next_send_valid_index_ += len;
  }
}
