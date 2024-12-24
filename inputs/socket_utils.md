# Network basics
(These are mostly copy & paste notes from various sources on the internet)
- the `iface_`. [A network interface controller (NIC, also known as a network interface card, network adapter, LAN adapter and physical network interface)](https://en.wikipedia.org/wiki/Network_interface_controller) is a computer hardware component that connects a computer to a computer network. The network interface card address, often referred to as MAC (for Media Access Control), is thus called the hardware address. It is protocol-independent and is usually assigned at the factory. The MAC in this code often gets assigned to the std:.string `iface_`.  
- the `ip_`. [While the NIC Card Manufacturer provides the MAC Address, the Internet Service Provider provides the IP Address](https://www.geeksforgeeks.org/difference-between-mac-address-and-ip-address/). The MAC or `iface_` identifies the physical device, it operates in the data link layer; while the `ip_` identifies the virtual (or logical) device, and it operates in the network layer. 
- the `port_`. Within an IP address, we may run multiple processes or services. For example, mail, web, database, etc. The `port_` number tells the network layer where the data (i.e., to/from which process) the incoming/outcoming data is coming to/from. The `port_` number thus allows us to identify a socket.
- `is_udp_`. [The internet protocol suite](https://www.freecodecamp.org/news/tcp-vs-udp/) is a collection of different protocols, or methods for devices to communicate with each other. Both TCP and UDP are major protocols within the internet protocol suite:
    * TCP, or Transmission Control Protocol, is the most common networking protocol online. TCP is extremely reliable, and is used for everything from surfing the web (HTTP), sending emails (SMTP), and transferring files (FTP). TCP is used in situations where it's necessary that all data being sent by one device is received by another completely intact. To establish a connection between two devices, TCP uses a method called a three-way handshake: For example, to read this article on your device, your device first sent a message to the freeCodeCamp News server called an SYN (Synchronize Sequence Number). Then the freeCodeCamp News server sends back an acknowledgement message called a SYN-ACK. When your device receives the SYN-ACK from the server, it sends an ACK acknowledgment message back, which establishes the connection. Once the three-way handshake is complete, the News server can start sending all the data your device's web browser needs to render this article. All devices break up data into small packets before sending them over the internet. Those packets then need to be reassembled on the other end. So when the freeCodeCamp News server sends the HTML, CSS, images, and other code for this article, it breaks everything into small packets of data before sending them to your device. Your device then reassembles those packets into the files and images it needs to render this article. TCP ensures that these packets all arrive to your device. If any packets are lost along the way, TCP makes it easy for your device to let the server know it's missing data, and for the server to resend those packets.
    * UDP. or User Datagram Protocol, is another one of the major protocols that make up the internet protocol suite. UDP is less reliable than TCP, but is much simpler. UDP is used for situations where some data loss is acceptable, like live video/audio, or where speed is a critical factor like online gaming. First, UDP is a connectionless protocol, meaning that it does not establish a connection beforehand like TCP does with its three-way handshake. Next, UDP doesn't guarantee that all data is successfully transferred. With UDP, data is sent to any device that happens to be listening, but it doesn't care if some of it is lost along the way. This is one of the reasons why UDP is also known as the "fire-and-forget" protocol. A good way to think about these differences is that TCP is like a conversation between two people. Person A asks person B to talk. Person B says sure, that's fine. Person A agrees and they both start speaking. UDP is more like a protester outside with a megaphone. Everyone who is paying attention to the protester should hear most of what they're saying. But there's no guarantee that everyone in the area will hear what the protester is saying, or that they're even listening.
- `is_listening_`: is the socket listening for new connections? Normally, server sockets may accept multiple client connections. Conceptually, a server socket listens on a known port. When an incoming connection arrives, the listening socket creates a new socket (the “child” socket), and establishes the connection on the child socket. The listening socket is then free to resume listening on the same port, while the child socket has an established connection with the client that is independent from its parent. One result of this architecture is that the listening socket never actually performs a read or write operation. It is only used to create connected sockets.

# Socket Utils
## SocketCfg
This is a struct to hold all that which specifies/characterises the socket:
- `iface_`, which identifies the network hardware;
- `ip_`, which identifies the network software, the socket "hub" in a way;
- `port_`, which identifies the socket or service within all sockets for the same `ip_` or "hub";
- `is_udp_`, which tracks whether the socket follows a TCP or UDP protocol;
- `is_listening_`, which tracks whether the socket is a  listening socket; and
- `needs_so_timestamp_`, which tracks whether we want to generate software timestamps when network packets hit the network socket.

## getIfaceIP
Returns the `iface_` number for a given name. For example, from interface name "eth0", returning the ip "123.123.123.123". 
- `getifaddrs` creates a linked list of `ifaddrs` structures, describing the network interfaces of the local system, and assigns the address of the first item of the list to `ifaddr`. On success, the function returns 0; on failure, it returns -1.
- for every `ifa` in the list of `ifaddrs` structures:
    * we check whether its address family matches that of the socket's. The AF_INET is the Internet family for IPv4 only.
    * we check whether the `ifa` name matches the input `iface`
If both conditions are true, then we found the `ifa` item for the input `iface`.
- `getnameinfo` then gets the socket address from the correct `ifa` and uses it to assign the ip string to `buf`. 
- we return `buf` and free the memory previously allocated for the list of `ifaddrs`.

## setNonBlocking
[In contrast to blocking sockets](https://dev.to/vivekyadav200988/understanding-blocking-and-non-blocking-sockets-in-c-programming-a-comprehensive-guide-2ien), non-blocking sockets operate asynchronously. When an I/O operation is initiated on a non-blocking socket, the program continues its execution immediately, regardless of whether the operation succeeds or not. This asynchronous behavior allows the program to perform other tasks while waiting for I/O operations to complete, enhancing overall efficiency and responsiveness.

During `setNonBlocking` we use `fcntl` to open the file description for the input descriptor `fd` and obtain its associated status flags; then check whether it contains the status flag 0_NONBLOCK; if not, add such flag to the file description.

## disableNagle
[Nagle's algorithm is used](https://www.packtpub.com/en-us/product/building-low-latency-applications-with-c-9781837639359) to improve buffering in TCP sockets and prevent overhead associated with guaranteeing reliability on the TCP socket. This is achieved by delaying some packets instead of sending them out immediately. For many applications, it is a good feature to have, but for low latency applications, disabling the latency associated with sending packets out is imperative.

For disabling Nagle's algorithm we use `setsockopt` on the input `fd`. `IPPROTO_TCP` corresponds to the level level at which the option resides (e.g., SOL_SOCKET for socket-level options or IPPROTO_TCP for TCP-specific options); and `TCP_NODELAY` is the option to disable packet delay.

## setSOTimestamp
We use this function when `needs_so_timestamp_` in the socket configuration is true, to allow the software to receive timestamps on the incoming packets.

## join
The `ip_mreq` structure is used with the `IP_ADD_MEMBERSHIP` in `setsockopt` for the local interface (socket with file descriptor `fd`) to join a multicast group with the input `ip` address and listen for multicast packets on that specific interface. 

## createSocket
(according to some configuration input via the SocketCfg `socket_cfg`)

- `getaddrinfo` takes a host (`ip`) and service (`port`), from the `socket_cfg`, together with a set of flags or conditions (an `addrinfo` structure called `hints` that allows to setup the UDP/TCP protocol for the new socket); and uses all of this data to obtain a list of candidate socket address structures. The first item in that list gets assigned to `result`. 

Then we loop over that list of `addrinfo` structures:
- We create a socket with file descriptor `socket_fd`;
- We make the socket non-blocking with `setNonBlocking`;
- If `socket_cfg` specifies a non-listening socket, then we **connect** the socket to the specified remote address (i.e., we establish some connection); otherwise, we set the correct flags, and associate (**bind**) the socket with its local address, so that clients can use that address to **listen** for incoming connections and eventually connect to some server;
- If the socket we want to build is TCP, then we call `disableNable`, to supress packet delays; and if it also is a listening socket, then we call `listen` on that socket - this marks the socket referred to by `socket_fd` as a passive socket, that is, as a socket that will be used to accept incoming connection requests using `accept`.
- If the socket is allowed to receive timestamps from the system, then we set the correct flags on its file descriptor `socket_fd` via `setSOTimestamp`.
- we return `socket_fd` with the correct settings according to the input `socket_cfg`.