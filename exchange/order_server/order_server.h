#pragma once

#include <functional>

#include "common/thread_utils.h"
#include "common/macros.h"
#include "common/tcp_server.h"

#include "order_server/client_request.h"
#include "order_server/client_response.h"
#include "order_server/fifo_sequencer.h"

namespace Exchange {
  class OrderServer {
  public:
    OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port);

    ~OrderServer();

    /// Start and stop the order server main thread.
    auto start() -> void;

    auto stop() -> void;

    /// Main run loop for this thread - accepts new client connections, receives client requests from them and sends client responses to them.
    auto run() noexcept {
      logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      while (run_) {
        // Detect events: incoming/outgoing message requests or new connection requests
        tcp_server_.poll(); 
        // Call sendAndRecv() on each socket in the server. 
        //  For each socket, it will allow it to either send data from its outbound_data_ buffer to the socket they're connected to; or to receive new data from the socket they're connected to into its inbound_data_ buffer.
        tcp_server_.sendAndRecv(); 
        
        // Now we use outgoing_responses_ from the order gateway server
        //   to repopulate some of the socket's outbound_data_ buffers
        for (auto client_response = outgoing_responses_->getNextToRead(); outgoing_responses_->size() && client_response; client_response = outgoing_responses_->getNextToRead()) {
          // Use client_id to obtain the sequence number
          auto &next_outgoing_seq_num = cid_next_outgoing_seq_num_[client_response->client_id_];
          logger_.log("%:% %() % Processing cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                      client_response->client_id_,    /* client_id  */
                      next_outgoing_seq_num,          /* sequence number */
                      client_response->toString());   /* client response */

          // Check that we have a socket linked to the client_id
          ASSERT(cid_tcp_socket_[client_response->client_id_] != nullptr,
                 "Dont have a TCPSocket for ClientId:" + std::to_string(client_response->client_id_));
          // If so, obtain the socket and call send() 
          //  - this will copy the sequence number into the client's socket outbound data buffer.
          cid_tcp_socket_[client_response->client_id_]->send(&next_outgoing_seq_num, sizeof(next_outgoing_seq_num));
          //  - this will copy the client response into the client's socket outbound data buffer.
          cid_tcp_socket_[client_response->client_id_]->send(client_response, sizeof(MEClientResponse));

          // Update the read index so that we jump into next response when 
          //  calling getNextToRead()
          outgoing_responses_->updateReadIndex();
          // Update sequence number so that every outgoing response is linked
          //    to a different sequence number
          ++next_outgoing_seq_num;
        }
      }
    }

    /// Read client request from the TCP receive buffer, check for sequence gaps and forward it to the FIFO sequencer.
    // When the OrderServer gets created, tcp_server_.recv_callback_ gets assigned this function
    // Note that the TCPServer recv_callback_ function gets called when calling each socket's sendAndRecv(). If a send_socket, the socket will send some data; if a receive_socket, the
    // socket will receive some data and later call recv_callback_ = the function below.
    //  It received some data, and that is a client request. So we wrap that data into some
    //  OMClientRequest data structure. If the socket that we received data from has not been 
    //  yet added to the list of client sockets, then add it. Check as well that the sequence
    //  number that we got from the client request is the one we expected.
    auto recvCallback(TCPSocket *socket, Nanos rx_time) noexcept {
      logger_.log("%:% %() % Received socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                  socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

      if (socket->next_rcv_valid_index_ >= sizeof(OMClientRequest)) {
        size_t i = 0;
        for (; i + sizeof(OMClientRequest) <= socket->next_rcv_valid_index_; i += sizeof(OMClientRequest)) {
          // Cast inbound_data from the client's socket into an OMClientRequest
          //  object that we can work on.
          auto request = reinterpret_cast<const OMClientRequest *>(socket->inbound_data_.data() + i);
          logger_.log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), request->toString());

          // If this is the first time we have a connection to this client,
          //  add the new connection to the list of clients
          if (UNLIKELY(cid_tcp_socket_[request->me_client_request_.client_id_] == nullptr)) { // first message from this ClientId.
            cid_tcp_socket_[request->me_client_request_.client_id_] = socket;
          }

          // If we have a socket linked to the client_id which is different
          //  to this client_request's socket.
          if (cid_tcp_socket_[request->me_client_request_.client_id_] != socket) { // TODO - change this to send a reject back to the client.
            logger_.log("%:% %() % Received ClientRequest from ClientId:% on different socket:% expected:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str_), request->me_client_request_.client_id_, socket->socket_fd_,
                        cid_tcp_socket_[request->me_client_request_.client_id_]->socket_fd_);
            continue;
          }

          auto &next_exp_seq_num = cid_next_exp_seq_num_[request->me_client_request_.client_id_];
          if (request->seq_num_ != next_exp_seq_num) { // TODO - change this to send a reject back to the client.
            logger_.log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str_), request->me_client_request_.client_id_, next_exp_seq_num, request->seq_num_);
            continue;
          }

          ++next_exp_seq_num;

          // Pass the client request to the FIFO sequencer.
          fifo_sequencer_.addClientRequest(rx_time, request->me_client_request_);
        }
        memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
        socket->next_rcv_valid_index_ -= i;
      }
    }

    /// End of reading incoming messages across all the TCP connections, sequence and publish the client requests to the matching engine.
    auto recvFinishedCallback() noexcept {
      fifo_sequencer_.sequenceAndPublish();
    }

    /// Deleted default, copy & move constructors and assignment-operators.
    OrderServer() = delete;

    OrderServer(const OrderServer &) = delete;

    OrderServer(const OrderServer &&) = delete;

    OrderServer &operator=(const OrderServer &) = delete;

    OrderServer &operator=(const OrderServer &&) = delete;

  private:
    const std::string iface_;
    const int port_ = 0;

    /// Lock free queue of outgoing client responses to be sent out to connected clients.
    ClientResponseLFQueue *outgoing_responses_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    Logger logger_;

    /// Hash map from ClientId -> the next sequence number to be sent on outgoing client responses.
    std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_outgoing_seq_num_;

    /// Hash map from ClientId -> the next sequence number expected on incoming client requests.
    std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_exp_seq_num_;

    /// Hash map from ClientId -> TCP socket / client connection.
    std::array<Common::TCPSocket *, ME_MAX_NUM_CLIENTS> cid_tcp_socket_;

    /// TCP server instance listening for new client connections.
    Common::TCPServer tcp_server_;

    /// FIFO sequencer responsible for making sure incoming client requests are processed in the order in which they were received.
    FIFOSequencer fifo_sequencer_;
  };
}
