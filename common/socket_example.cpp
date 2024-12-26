#include "time_utils.h"
#include "logging.h"
#include "tcp_server.h"

int main(int, char **) {
  using namespace Common;

  std::string time_str_;
  Logger logger_("/usr/outputs/socket_example.log");

  auto tcpServerRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
    logger_.log("TCPServer::defaultRecvCallback() socket:% len:% rx:%\n",
                socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

    const std::string reply = "TCPServer received msg:" + std::string(socket->inbound_data_.data(), socket->next_rcv_valid_index_);
    socket->next_rcv_valid_index_ = 0;

    socket->send(reply.data(), reply.length());
  };

  auto tcpServerRecvFinishedCallback = [&]() noexcept {
    logger_.log("TCPServer::defaultRecvFinishedCallback()\n");
  };

  auto tcpClientRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
    const std::string recv_msg = std::string(socket->inbound_data_.data(), socket->next_rcv_valid_index_);
    socket->next_rcv_valid_index_ = 0;

    logger_.log("TCPSocket::defaultRecvCallback() socket:% len:% rx:% msg:%\n",
                socket->socket_fd_, socket->next_rcv_valid_index_, rx_time, recv_msg);
  };

  const std::string iface = "lo";
  const std::string ip = "127.0.0.1";
  const int port = 12345;

  logger_.log("Creating TCPServer on iface:% port:%\n", iface, port);
  TCPServer server(logger_);
  // Initialise both function wrappers to the above lambda expressions
  server.recv_callback_ = tcpServerRecvCallback;
  server.recv_finished_callback_ = tcpServerRecvFinishedCallback;
  server.listen(iface, port);
  
  std::vector<TCPSocket *> clients(5);

  for (size_t i = 0; i < clients.size(); ++i) {
    // Create one socket (one client)
    clients[i] = new TCPSocket(logger_);
    // Initialise its wrapper function with lambda above
    clients[i]->recv_callback_ = tcpClientRecvCallback;

    logger_.log("Connecting TCPClient-[%] on ip:% iface:% port:%\n", i, ip, iface, port);
    // When client connects to socket with "iface" and "port" attributes, calls inside createSocket() with "iface" and "port", and the returned fd is kept inside the client as "the socket that I am connected to".
    clients[i]->connect(ip, iface, port, false);
    // Client connects to socket with "iface" and "port" attributes, which is the listener's socket.
    server.poll();
    // With poll(), the queue of connections to listener gets distributed between "receiver" and "sender" sockets
    // In this case, we just have listener
  }

  using namespace std::literals::chrono_literals;

  for (auto itr = 0; itr < 5; ++itr) {
    for (size_t i = 0; i < clients.size(); ++i) {
      // Build message and log it
      const std::string client_msg = "CLIENT-[" + std::to_string(i) + "] : Sending " + std::to_string(itr * 100 + i);
      logger_.log("Sending TCPClient-[%] %\n", i, client_msg);
      
      // Copy data (message) into server's outbound_data buffer
      // e.g.:  Sending TCPClient-[0] CLIENT-[0] : Sending 0
      clients[i]->send(client_msg.data(), client_msg.length());
      // Client sends data from outbound_data buffer 
      // e.g.:  /usr/src/tcp_socket.cpp:104 sendAndRecv() Sat Oct 26 15:31:26 2024 send socket:6 len:22
      //      clients[0] sends data from its outbound_data buffer to socket with fd=6. Data length is 22.
      clients[i]->sendAndRecv();

      std::this_thread::sleep_for(500ms);

      // Distribute fds in the epoll list between sockets to receive data from/send data to
      // e.g.;  /usr/src/tcp_server.cpp:99 poll() Sat Oct 26 15:31:26 2024 EPOLLIN socket:7
      //      Server finds one socket that is sending data (EPOLLIN). This is socket with fd=7 (client[0])     
      server.poll();
      
      // Server loops over all sockets to receive data from and to send data to. 
      // e.g.:  /usr/src/tcp_socket.cpp:91 sendAndRecv() Sat Oct 26 15:31:26 2024 read socket:7 len:22 utime:1729956686935088965 ktime:1729956686434806000 diff:500282965
      //      There's only one socket who is sending data to server. Reads data from socket with fd=7.
      server.sendAndRecv();
    }
  }

  return 0;
}
