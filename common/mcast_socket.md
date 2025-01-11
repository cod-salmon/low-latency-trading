# The McastSocket
Against the TCPSocket class, this is a socket class to allow connections over UDP. Recall that UDP is a connectionless protocol, meaning that it does not establish a connection beforehand like TCP does; data is sent to any device that happens to be listening, but the protocol doesn't care if some of it is lost along the way. 

As with the `TCPSocket`, it contains:

- an `outbound_data_` buffer to hold data to be sent by the socket;
- a `next_send_valid_index_` index that keeps track of the next index from where to keep adding data into `outbound_data_`;
- an `inbound_data_` buffer to receive data into the socket;
- a `next_rcv_valid_index_` index to keep track of the next index from where to keep adding data into `inbound_data_`.
- a `socket_fd` file descriptor for the socket we are connected to;
- a `recv_callback_` `std::function` variable that, if setup, allows to run a piece of code during `McastSocket::sendAndRecv` if new data has been received into the socket (e.g., for printing some additional message in the log).

## `McastSocket::sendAndRecv`
It is very similar to `TCPSocket::sendAndRecv`. The difference is that 
- in the `TCPSocket` class we use `recvmsg` to receive data from `socket_fd` into the socket. This function needs `inbound_data_` to be wrapped inside a `iovec` type, wrapped inside a `msghdr` type. On the other hand, in the `McastSocket` class we use `recv`, which uses `inbound_data_` directly to copy data in from `socket_fd_`, which simplifies the code much more.
- in the `TCPSocket` class we also measure the time it took to read the data, which gets input into the slightly different `recv_callback_` `std::function`.

## `McastSocket::send`
Same as `TCPSocket::send`: copies `data` to be sent into the socket's `outbound_data_`.

## `McastSocket::init`, `::join` and `::leave`
Initialises `socket_fd_`, the file descriptor of the socket we use to connect to the multicast stream. Through `McastSocket::join`, we join `socket_fd_` to the multicast stream at `ip`; and through `McastSocket::leave` we drop the membserhip to the multicast stream at `ip` and close `socket_fd_`.




