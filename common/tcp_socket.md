# The TCPSocket 
A `TCPSocket` class consists of:
- a `socket_fd`, holding the file descriptor for the socket the TCPSocket is connecting to/listening on;
- two buffers called `outbound_data_` and `inbound_data_`, for data that is meant to be sent from the TCPSocket and data that is meant to be received onto the TCPSocket respectively; and with their corresponding `next_send_valid_index_` and `next_rcv_valid_index_` indices, to track their next available space.
- a  `sockaddr_in` strut, to hold the attributes necessary for making the socket an IPv4 type. This struct gets setup during `TCPScoket::connect` and only used during `TCPSocket::sendAndRecv` to transfer the data from `socket_fd_` onto the `inbound_data_`.
- a std::function variable called  `recv_callback_`, which is used by the TCPSocket to acknowledge to the `socket_fd_` socket that data has been received.

## The TCPSocket implementation 
Imagine we have one socket, `socket_a`. First thing we do is to establish a connection of `socket_a` with another socket, `socket_b`. For that we use `socket_a`'s member functions:
### `TCPSocket::connect`
`socket_a`'s `connect` instatiates a `socket_cfg` with the input `ip`, `iface`, `port` and `is_listening` variables (that specify `socket_b`); then it uses that SocketCfg in `createSocket` to obtain the file descriptor for `socket_b` (sort of the ID for `socket_b`), and assigns that file descriptor or ID to it's internal member variable `socket_fd_` (so that it can use it everytime it wants to communicate with `socket_b`). 

Once a connection gets established between `socket_a` and `socket_b`, `socket_a` is willing to start a conversation with `socket_b` so gets its message ready (imagine, "Show must go onnnnn"): 

### `TCPSocket::send` 
It's used to copy the message "Show must go onnnnn" into the `socket_a`'s `outbound_data_` buffer (this is the place where messages from `socket_a` wait to be sent to `socket_b`).

### `TCPSocket::sendAndRecv`
1. **`socket_b` says "hi" to `socket_a`**: We take `inbound_data_` and the next available space inside `inbound_data_` to create an `iovec` struct called `iov`. This is used as empty memory able to hold data the size of (the maximum buffer size `TCPBufferSize`- the next available space in `inbound_data_`). This struct gets wrapped inside another `msghdr` struct called `msg`, with which `recvmsg` works. Via `recvmsg`, data from the socket with file descriptor `socket_fd_` (the "hi" message from `socket_b`) gets copied into `socket_a`'s `inbound_data_` through the `msg` wrapper. The output from `recvmsg` is `read_size`, the size of the data which was copied into `inbound_data_`. Therefore, `next_rcv_valid_index` gets increased by `read_size`. 
2. **`socket_a` waves back to `socket_b`**: `recv_callback_` is a function variable, taking a TCPSocket and some time variable as parameters. What the function does must be specified by the user when implementing the TCPSocket instance.  However, we should expect `next_rcv_valid_index_` to be set to zero when `recv_callback_` is called; as well as a `send` call to acknowlege that the message has been received on `socket_a`.
3. **`socket_a` sends its message "Show must go onnnnn" to `socket_b`**: if we still have data to be sent (`next_send_valid_index_` is greater than zero), then we use `::send` to transfer data (or the message) from the `outbound_data_` buffer to the socket with file descriptor `socket_fd_` and set `next_send_valid_index_` to zero.

Note the `::send` function above is not the same as the `TCPSocket::send` function, which is used to copy input `data` into the `outbound_data_` buffer to be sent during `TCPSocket::sendAndRecv`. Therefore, we should always call `TCPSocket::send` before `TCPSocket::sendAndRecv`, so that the latter acts on the updated `outbound_data_` buffer.

# The TCPServer
Inside a TCPServer we have:
- a TCPSocket called `listener_socket_`, which holds the TCPSocket where the TCPServer listens for new connections. When a new connection request is found, a new TCPSocket gets created and the connection established.
- When a connection gets established, two new TCPSockets get added, one on `receive_sockets_` and another on `send_sockets_`. 
- an `epoll_fd_` file descriptor for an `epoll` instance; and a `epoll_event` buffer called `events_`. These `events_` are connection requests gathered by the epoll instance with file descriptor `epoll_fd_` in `epoll_wait` (during `TCPServer::poll`).
- Two std::function variables called `recv_callback_` (which gets used by each of the individual TCPSockets in the server for their `recv_callback_`) and  `recv_finished_callback_` (which gets used by the server once all data has been read in and sent out).

## The TCPServer implementation

### `TCPServer::addToEpollList`
Adds input TCPSocket to the interest list of the epoll instance referred to by the file `epoll_fd_`. 

### `TCPServer::listen`
Can be read as "listen in here" (i.e., "I want the server to listen through its `listener_socket_` on this `iface` and `port`"): creates an epoll instance and assigns its file descriptor to `epoll_fd_`. Then it places the `listener_socket_` on the input `iface` and `port`; and adds `listener_socket_` to the epoll list (or interest list of the epoll instance).

[The epoll API](https://linux.die.net/man/7/epoll) monitors multiple file descriptors to see if I/O is possible on any of them. The central concept of the epoll API is the epoll instance, an in-kernel data structure which, from a user-space perspective, can be considered as a container for two lists:
    * The interest list (sometimes also called the epoll set): the set of file descriptors that the process has registered an interest in monitoring.
    * The ready list: the set of file descriptors that are "ready" for I/O.  The ready list is a subset of (or, more precisely, a set of references to) the file descriptors in the interest list.  The ready list is dynamically populated by the kernel as a result of I/O activity on those file descriptors.

### `TCPServer::sendAndRecv`
Calls `TCPSocket::sendAndRecv` first on `receive_sockets_`, gathering data; then on `send_sockets_`, to send data.

### `TCPServer::poll`
Gathers all I/O requests from the file descriptors that the epoll instance associated to `epoll_fd_` keeps track of inside `events_`.

Then it loops over each of those `events_`. For every `epoll_event`, gets the associated TCPSocket for such event/request; then:
- If the request is an EPOLLIN or input request, then we add the TCPSocket to `receive_sockets_` (sockets that the TCPServer is receiving data on).
- If the request is an EPOLLOUT or output request, then we add the TCPSocket to `send_sockets_` (sockets which the TCPServer sends data from).
- If the request is an EPOLLIN or input request, and the associated socket corresponds to the `listener_socket_`, then we are having a new connection request. This breaks up the `events_` loop and moves to an infinite while loop meant to handle all new connection requests inside the `listener_socket_`: for every connection request, the code creates a new TCPSocket and adds it to the interest list of the TCPServer epoll instance to check for I/O events. If the new socket is still not in `receive_sockets_`, we add it. The while loop breaks once we finish with all connection requests.

Therefore, during `TCPServer::poll` one focusses either on the epoll "interest list", adding to it all new connection requests inside the `listener_socket_`; or on the epoll "ready list", setting up the `send_sockets_` or `receive_sockets_` containers.

# The `socket_example.cpp`
In this example, a TCPServer `server` gets created and its `listener_socket_` assigned to `iface` and `port`, where the `server` will listen to new connections from. The callbacks for both the `server` and its `clients` are set up. 

A vector of five TCPSocket pointers is created, which get instatiated inside a for-loop:
1. We instatiate `clients[i]` with a fresh new TCPSocket;
2. We connect `clients[i]` to the server's listening socket, specified by `iface` and `port`. 
3. This connection request gets detected during `server.poll()` as an EPOLLIN event on the listening socket. We then create a new socket and add it to the "interest list" of the `server`'s epoll instance as well as to the `server`'s `receive_sockets_`.

Once the server and its five clients are setup, we run through five iterations; looping over all five `clients` in each of them:
    * each client calls `TCPSocket::send` to place its message into its `outbound_data_` buffer;
    * then calls `TCPSocket::sendAndRecv` to send the message from the `outbound_data_` to the socket `socket_fd_` it is connected to;
    * then we call `TCPServer::poll` to distribute the sockets between senders (`client[i]`) and receivers (its `socket_fd_`).
    * then we call `TCPServer:: sendAndRecv` to let senders send their messages and receivers to receive their messages.

### e.g., connection (`clients[0]`-`socket_fd_ = 6`).

First iteration, when `itr = 0`, from `outputs/socket_example.log` (starting on line 33). The log begins with `Sending TCPClient-[0]`; then it prints the message ready to be sent by `client[0]`: `CLIENT-[0] : Sending 0` through `TCPSocket::send`:
```
Sending TCPClient-[0] CLIENT-[0] : Sending 0
```

Then it passes the message of length 22 during `sendAndRecv()` to the socket it is connected to, the socket with file descriptor = 6:

```
/usr/src/tcp_socket.cpp:77 sendAndRecv() Wed Dec 18 11:22:31 2024 send socket:6 len:22
```

In the next iteration, `itr = 1`, once we are done with all `clients`, we see again (line 71) `client[0]` sending the message "CLIENT-[0] : Sending 100" (a message of length 24) to socket with file descriptor = 6:

```
/usr/src/tcp_socket.cpp:77 sendAndRecv() Wed Dec 18 11:22:34 2024 send socket:6 len:24
```

Before that (on line 68), `clients[0]` will be reading a message of length 45 from socket with file descriptor = 6. This (callback) message is: "TCPServer received msg:CLIENT-[0] : Sending 0", the confirmation from socket with file descriptor = 6 that the message "CLIENT-[0] : Sending 0" has been received:

```
Sending TCPClient-[0] CLIENT-[0] : Sending 100
/usr/src/tcp_socket.cpp:63 sendAndRecv() Wed Dec 18 11:22:34 2024 read socket:6 len:45 utime:1734520954332319712 ktime:1734520952330958000 diff:2001361712
TCPSocket::defaultRecvCallback() socket:6 len:0 rx:1734520952330958000 msg:TCPServer received msg:CLIENT-[0] : Sending 0
```

After sending the message from `clients[0]`, we call `TCPServer::poll`. This call collects all events within the server and distributes them between `send_sockets_` and `receive_sockets_`. In this case, we find socket with file descriptor = 7 sending data within the server (a `receive_socket_`, or socket we receive data from):
```
/usr/src/tcp_server.cpp:72 poll() Wed Dec 18 11:22:32 2024 EPOLLIN socket:7
```

After `TCPServer::sendAndRecv`, the message from socket with file descriptor = 7 is read:
```
/usr/src/tcp_socket.cpp:63 sendAndRecv() Wed Dec 18 11:22:32 2024 read socket:7 len:22 utime:1734520952330893889 ktime:1734520951830708000 diff:500185889
TCPServer::defaultRecvCallback() socket:7 len:22 rx:1734520951830708000
```

and a callback message is sent to socket with file descriptor = 7:
```
/usr/src/tcp_socket.cpp:77 sendAndRecv() Wed Dec 18 11:22:32 2024 send socket:7 len:45
TCPServer::defaultRecvFinishedCallback()
```

Therefore, `clients[0]` is the socket with file descriptor = 7, which is communicating with another socket with file descriptor = 6:
1. `clients[0]` sends the message "CLIENT-[0] : Sending 100" to `socket_fd_` = 6;
2. During `TCPServer::poll` and `TCPServer::sendAndRecv`, `clients[0]` (with file descriptor = 7) is detected to have some data to be read. 
3. That data is read by socket with file descriptor = 6; which sends a callback message to `clients[0]` (as seen later in the log).

Note that file descriptors 7,9,11,13 and 15 are associated to `clients`, while file descriptors 6,8,10,12 and 14 are associated to the sockets they're connected to.