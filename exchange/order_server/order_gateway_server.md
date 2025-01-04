# The Order Gateway Server
The Order Gateway Server provides a TCP server or interface for clients to connect to and communicate with the matching engine. It 
- processes incoming client requests and forwards them to 
the matching engine;
- receives order responses from the matching engine and forwards them to the correct client.

## Requests/Responses in the Order Gateway Server
Therefore, we use two different types of structures:
- `MEClientRequest`/`MEClientResponse` are two structs used for the matching engine to communicate with the order gateway server;
- `OMClientRequest`/`OMClientResponse` are two structs used for clients to communicate with the order gateway server.

### MEClientRequests
In the `client_request.h` file we have the following:
- A **client request**: A definition of the `MEClientRequest` struct. Every instance of this struct will hold specific values for some of the typedefs from `common/types.h`, so that to have a complete request message such as: "Client `client_id_`, making `type_` (new or cancellation) request on the existing (or new) order `order_id_` to buy/sell (depending on `side_`) the amount `qty_` of stocks `ticker_id_` at the price `price_`".
- A **to-string method for the client request**. The `MEClientRequest` struct has a `toString`  method to print itself (the `common/types.h` values that characterise it).
- A **client request type**. An enum class called `ClientRequestType` which can be either INVALID, NEW (the client has a new order request) or CANCEL (the client wishes to cancel a buy/sell order request).
- A **to-string method for the client request type**. 

We also define a new type of container called `ClientRequestLFQueue`, which is an `LFQueue` (see `common/lf_queue.h`) holding `MEClientRequest` objects.

### MEClientResponses
In the `client_response.h` file we have the following:
- A **client response**: A definition of the `MEClientResponse` struct. It is similar to `MEClientRequest`, although compared to `MEClientRequest`, `MEClientResponse` has two `OrderId` instances and two `Qty` instances:
    - `MEClientRequest` has `order_id` (referring to the client order id); while `MEClientResponse` has `client_order_id` to refer to the same thing but to distinguish it from the other `OrderId` `market_order_id`.
    - `MEClientRequest` has `qty_` (referring to quantity the client wants to buy/sell); while `MEClientResponse` has `exec_qty_` and `leaves_qty_`, referring to the final execution and remaining quantities after the execution.
- A **to-string method for the client response**. The `MEClientResponse` struct has a `toString`  method to print itself.
- A **client response type**. An enum class called `ClientResponseType` which can be 
    - ACCEPTED - the request has been accepted by the matching engine; 
    - CANCELED - the matching engine has successfully cancelled the order the  client requested.
    - FILLED - the matching engine has been able to successfully fill the client's request (buy/sell);
    - CANCEL_REJECTED - when the cancel request from the client comes too late; and 
    - INVALID.
- A **to-string method for the client request type**. 

We also define a new type of container called `ClientResponseLFQueue`, which is an `LFQueue` (see `common/lf_queue.h`) holding `MEClientResponse` objects.

### OMClientRequests/OMClientResponses
While `MEClientRequest`/`MEClientResponse` are two structs used for the matching engine to communicate with the order gateway server, `OMClientRequest`/`OMClientResponse` are two structs used for clients to communicate with the order gateway server. Therefore, an `OMClientRequest`/`OMClientResponse` contains an instance `me_client_request_`/`me_client_response_` together with a `seq_num_`. This is the sequence number field. Every struct/class used to communicate between the outside (market/clients) and the exchange should contain a sequence number that allow market participants/clients to detect gaps in the information they get from the exchange.

## The FIFOSequencer
The FIFOSequencer deals with clients requests through two containers:
- `incoming_requests_`, which is a ClientRequestLFQueue type (an LFQueue of MEClientRequest objects), which holds the already processed requests to be sent to the matching engine; and
- `pending_client_requests_`, which is an array of RecvTimeClientRequest structs, which holds the requests yet to be processed and put into `incoming_requests_`.

The `RecvTimeClientRequest` struct is defined inside the `FIFOSequencer` class to help processing requests in the right order, by pairing a request with its arrival time to the order gateway server. During `FIFOSequencer::sequenceAndPublish`, `pending_client_requests_` sorts the client requests (wrapped inside the `RecvTimeClientRequest` struct), according to their arrival time. Then, one by one, adds each `client_request` to the `incoming_requests_`. How requests get added to `pending_client_requests_` in the first place, is through `FIFOSequencer::addClientRequest`. `pending_size_` tracks the next available space to add a request to `pending_client_requests_`. As long as `pending_size_` is not greater than the assigned size for the `pending_client_requests_` container, we wrap the new `request` inside a `RecvTimeClientRequest`, together with its arrival time `rx_time`, and add the it to the `pending_size_++` position in `pending_client_requests_`.

# The `OrderServer` class
Contains:
- a `tcp_server_` instance, to allow clients to connect to the matching engine;
- a `iface_` and `port_` to listen for new connections on the `tcp_server_`;
- an array `cid_tcp_socket_`, mapping a `client_id` (given by the array index) to a TCPSocket pointer (note that different `client_id`s may map to the same TCPSocket pointer);
- an array `cid_next_exp_seq_num_`, mapping `client_id` to the next sequence number expected on incoming client requests;
- an array `cid_next_outgoing_seq_num_`, mapping `client_id` to the next sequence number to be sent on outgoing client responses;
- a `fifo_sequencer_`, to make sure incoming client requests are processed by the matching engine in the order in which they are received in the exchange;
- a `outgoing_responses_`, to deliver `MEClientResponses` from the matching engine to their respective clients.

## `OrderServer::recvCallback`
We take the input `socket` and access its `next_rcv_valid_index_`. This tells us the size of the socket's incoming data. We split that data into packets the size of a `OMClientRequest` struct. For each packet, we generate a new `request` and add it to the `fifo_sequencer_`. However, before calling `FIFOSequencer::addCLientRequest`, we perform a few checks on `request`:
(1) If this the first time we interact with this client, then we add its `client_id`-`socket` pair to `cid_tcp_socket_`;
(2) If the socket we have assigned to `client_id` does not match the current `socket`, then we skip this request and move on to the next, if available (recall that within `socket` we may have different requests from different clients; or different `client_id`s may map to the same `socket`, so we might find a different situation in the next iteration);
(3) If the `next_exp_seq_num`, that we extract from our `cid_next_exp_seq_num_` does not match the `request`'s `seq_num`, then we skip this request.

Only when we reach beyond check (3), we increment `next_exp_seq_num` and add `request` to the `fifo_sequencer_` to be processed.

## `OrderServer::recvFinishedCallback`
While `OrderServer::recvCallback` gathers client requests and adds them to the `fifo_sequencer_`, `OrderServer::recvFinishedCallback` calls  `FIFOSequencer::sequenceAndPublish` (described above and on the comments at `fifo_sequencer.h`) to actually sort out and send the client requests to the matching engine.

## `OrderServer::run`
We have a call to:
- `TCPServer::poll`, which gathers all EPOLLIN and EPOLLOUT events in the existing sockets in the server and arranges them between `receive_sockets_` and `send_sockets` and/or checks for new connection requests and adds them to `receive_sockets_`.
- `TCPServer::sendAndRecv`, which calls `TCPSocket::sendAndRecv` first on `receive_sockets_`, gathering data; then on `send_sockets_`, to send data. Note that, during `TCPServer::sendAndRecv` on `receive_sockets_`, a call to `OrderServer::recvCallback` is made, where requests from clients are gathered and processed by the `fifo_sequencer_`.

Because now all sockets that had something to say have just sent their messages out, we can loop over `outgoing_responses_` and copy each `client_response` to its respective socket's `outbound_data_` buffer (data from the buffer will be sent out in the next call to `TCPServer::sendAndRecv`). Before that, for each `client_response` we obtain its `next_outgoing_seq_num` from our records at `cid_next_outgoing_seq_num_`, and check that there is a socket linked to the `client_response`'s `client_id` on `cid_tcp_socket_`. If so, we copy, first, the `next_outgoing_seq_num`, then the `client_response`, into that socket's `outbound_data_`. Recall that the `next_outgoing_seq_num` is what will allow the client to know whether it is on sync with the exchange. 

## `OrderServer::start` and `OrderServer::stop`
`OrderServer::run` contains a while-loop which gets switched on/off with the `run_` variable. On `OrderServer::start`, `run_` is set to true, and a `std::thread` instance gets created where the `OrderServer::run` process gets run. The process on that thread will finish only when `run_` is set to false by `OrderServer::stop`, even if the `std::thread` instance gets destroyed during `OrderServer::start`. The bearable issue of having the `std::thread` instance destroyed before its assigned `run` process is finished, is that we don't know (in code terms) when the process has ended (vs. having a call somewhere to `std::thread::join()`, which makes sure the instance gets destroyed after its assigned process is finished). 