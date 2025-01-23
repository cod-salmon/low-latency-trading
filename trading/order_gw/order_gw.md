# The OrderGateway
This is the OrderGateway that a specific client is using to trade her assets. Therefore, every instance of an OrderGateway will be assigned to some `client_id_`.  The OrderGateway:
1. Keeps a TCP connection (`tcp_socket`) to the order gateway server in the exchange side infrastructure (hence, the necessary `ip_`, `iface_` and `port_` variables), and the `size_t` variables `next_outgoing_seq_num_` and `next_exp_seq_num_`, that keep track of the next sequential number to assign to the packet send forward from the OrderGateway to the exchange, and of the next sequential number we expect from the packet received from the exchange.
2. Communicates with the trading engine back and forth via the LFQueues `outgoing_requests_` (for requests from the client to the trading engine) and `incoming_responses_` (for responses from the trading engine to the client).

When we hit `OrderGateway::start`, we connect the OrderGateway `tcp_socket_` to the exchange (with `ip_`, `iface_` and `port_`), and start a thread with `OrderGateway::run`.

## `OrderGateway::run`
We call `TCPSocket::sendAndRecv`, which reads in data from the `tcp_socket_`'s `inbound_data_` buffer and reads outs data from the `tcp_socket_`'s`outbound_data_` buffer. If any data gets received then `OrderGateway::recvCallback` gets called.

Then we loop over `outgoing_requests_`, to send the MEClientRequests to the exchange.

## `OrderGateway::recvCallback`
New data was added onto the `tcp_socket_`'s `inbound_data_` buffer. We split it and wrap it into packets with the size and form of a `OMClientResponse` struct. Then, for each of these packets we:
- check that the `OMClientResponse`'s `client_id_` matches the `OrderGateway::client_id_`; and
- check that the `OMClientResponse`'s `seq_num_` matches the expected sequence number, `next_exp_seq_num_`.
If we pass both checks, then we forward the `OMClientResponse`'s `MEClientResponse` to `incoming_responses_` for the trade engine to read.
