In the `main_exchange.cpp` we have three actors: the `matching_engine`, the `market_data_publisher` and the `order_server`; and three communication media: `client_requests` (from `order_server` to `matching_engine`), `client_responses` (from `matching_engine` to `order_server`), and `market_updates` (from `matching_engine` to `market_data_publisher`).

After these have been defined, we see the following:
```
logger->log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
```
which matches with the first thing we see in `exchange_main.log`:
```
/usr/src/exchange/exchange_main.cpp:44 main() Mon Jan 13 00:35:21 2025 Starting Matching Engine...
```
Then the `matching_engine` gets initialised with the `LFQueue`s for `client_requests`, `client_responses` and `market_updates` and we trigger `MatchingEngine::start`.

Often, the `matching_engine` will exist on its own. Every `order_book` instance would hold a link to this `matching_engine` through a private MatchingEngine pointer. The `matching_engine` itself on the other hand, would hold a OrderBookHashMap, or an array of links (or pointers) to all `order_book`s, for each `ticker_id`.

A `matching_engine` would first be created, and then `MatchingEngine::start` would trigger `MatchingEngine::processClientRequest`. This would make the `matching_engine` to loop over the `incoming_requests_`. For each `incoming_request` it will check its `ticker_id` and grab its respective `order_book`. If the request is of type NEW (new order), then the `matching_engine` will call `MEOrderBook::add`, to add the `incoming_request`'s order; if the request is of type CANCEL (cancel order), then the `matching_engine` will call `MEOrderBook::cancel` to cancel the `incoming_request`'s order.

At the same time, when `MEOrderBook::add` is called, the respective `order_book` makes use of the `matching_engine` to send responses to the clients and updates to the market (through `MatchingEngine::sendClientResponse` and `MatchingEngine::sendMarketUpdate` respectively).

In this `exchange_main.cpp` example, when we call `MatchingEngine::start`, we hit it with an empty queue of `incoming_requests`; so not much goes on. Even if the queue was not empty, the `MatchingEngine::ticker_order_book_` is, as we have not created any MEOrderBook instances. That is why, you can only see
```
/usr/src/exchange/matcher/matching_engine.h:64 run() Mon Jan 13 00:35:23 2025

```
at `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_matching_engine.log`.

Next, in `exchange_main.cpp`, we define some variables to help us setup the network interface for UDP streaming in the MarketDataPublisher. Then, we create an instance of a MarketDataPublisher with those variables and hit `MarketDataPublisher::start`.  That is why we see
```
/usr/src/exchange/exchange_main.cpp:52 main() Mon Jan 13 00:35:24 2025 Starting Market Data Publisher...
```
at `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_main.log`, and why you see 
```
/usr/src/common/socket_utils.h:112 createSocket() Mon Jan 13 00:35:25 2025 configuration:SocketCfg[ip:233.252.14.3 iface:lo port:20001 is_udp:1 is_listening:0 needs_SO_timestamp:0]
/usr/src/exchange/market_data/market_data_publisher.cpp:15 run() Mon Jan 13 00:35:26 2025
```
at `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_market_data_publisher.log`: When a `MarketDataPublisher` gets created, an `incremental_socket_` gets created, to broadcast the market updates coming from the `matching_engine`.  The `MarketDataPublisher` also creates a new SnapshotSynthesizer and keeps a link to it through its private `snapshot_synthesizer_` pointer (* see below). Then we hit `MarketDataPublisher::start`, which triggers `MarketDataPublisher::run`, and the MarketDataPublisher starts processing the `MarketDataPublisher::outgoing_md_updates_`, (a link or pointer to an external instance of a MEMarketUpdateLFQueue, which holds the updates that the `matching_engine` wishes to communicate to the market). Each MEMarketUpdate from `outgoing_md_updates_` is passed onto the `incremental_socket_` for incremental UDP streaming; and to its internal MDPMarketUpdateLFQueue instance, called `snapshot_md_updates_`. Changes to `snapshot_md_updates_` get immediately picked-up by the `MarketDataPublisher::snapshot_synthesizer_` to be added to the next snapshot.

(* see above) When the SnapshotSynthesizer gets created during the instantiation of the MarketDataPublisher, the log file passed onto the SnapshotSynthesizer, is also passed onto its `SnapshotSynthesizer::snapshot_socket_`. Therefore, unlike `MarketDataPublisher::incremental_socket_`, for which we have no output in any of the log files, we have some output from the `SnapshotSynthesizer::snapshot_socket_` at `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_snapshot_synthesizer.log`.

In the first few lines of `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_snapshot_synthesizer.log` we see how `Common::createSocket` gets called via `McastSocket::init`, initialising `SnapshotSynthesizer::snapshot_socket_`. Then `SnapshotSynthesizer::run` gets called. Note this gets triggered when calling `SnapshotSynthesizer::start` through  `MarketDataPublisher::start`. 

During `SnapshotSynthesizer::run`, we loop over `SnapshotSynthesizer::snapshot_md_updates_` (which are continuously updated during `MarketDataPublisher::run`). For each MDPMarketUpdate from `SnapshotSynthesizer::snapshot_md_updates_`, we call `SnapshotSynthesizer::addToSnapshot`. Because `exchange_main.cpp` is a very simple example, we really have no `MDPMarketUpdate`s. Each `MDPMarketUpdate` memory at `SnapshotSynthesizer::snapshot_md_updates_` is empty (or nullptr), so when calling `SnapshotSynthesizer::addToSnapshot` to each of them, they will be of `MarketUpdateType::INVALID`, and so nothing will be assigned to `SnapshotSynthesizer::ticker_orders_` neither. Instead, `SnapshotSynthesizer::ticker_orders_` will be a `ME_MAX_TICKERS`-size array of arrays of empty `MEMarketUpdate` memories. 

Therefore, when calling `SnapshotSynthesizer::publishSnapshot` during `SnapshotSynthesizer::run`, we 
(1) Send `start_market_update` to the `snapshot_socket_`:
```
2025 MDPMarketUpdate [ seq:0 MEMarketUpdate [ type:SNAPSHOT_START ticker:INVALID oid:0 side:INVALID qty:INVALID price:INVALID priority:INVALID]]
``` 
(2) Send a `me_market_update` for each int in the range 0 to `ME_MAX_TICKERS`. This constexpr was set equal to 8 at `common/types.h`, that is why we see the same message 
```
/usr/src/exchange/market_data/snapshot_synthesizer.cpp:96 publishSnapshot() Mon Jan 13 00:35:27 2025 MDPMarketUpdate [ seq:1 MEMarketUpdate [ type:CLEAR ticker:0 oid:INVALID side:INVALID qty:INVALID price:INVALID priority:INVALID]]
```
up to "type:CLEAR ticker:7" in `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_snapshot_synthesizer.log`:
```
2025 MDPMarketUpdate [ seq:8 MEMarketUpdate [ type:CLEAR ticker:7 oid:INVALID side:INVALID qty:INVALID price:INVALID priority:INVALID]]
```
(3) Send a `end_market_update` to the `snapshot_socket_`:
```
/usr/src/exchange/market_data/snapshot_synthesizer.cpp:122 publishSnapshot() Mon Jan 13 00:35:27 2025 MDPMarketUpdate [ seq:9 MEMarketUpdate [ type:SNAPSHOT_END ticker:INVALID oid:0 side:INVALID qty:INVALID price:INVALID priority:INVALID]]
```
Finally, we finish with the `McastSocket::sendAndRecv` call:
```
/usr/src/common/mcast_socket.cpp:38 sendAndRecv() Mon Jan 13 00:35:27 2025 send socket:8 len:420
```
and the final log comment:
```
src/exchange/market_data/snapshot_synthesizer.cpp:126 publishSnapshot() Mon Jan 13 00:35:27 2025 Published snapshot of 9 orders.
```

Then, in `exchange_main.cpp`, we define some variables to setup the TCP server that underlies the OrderServer class and create an `order_server` instance:
```
/usr/src/exchange/exchange_main.cpp:59 main() Mon Jan 13 00:35:28 2025 Starting Order Server...
```

When we hit `OrderServer::start` and hence `OrderServer::run`, the OrderServer will first collect all events (incoming/outgoing requests) in the server within its sockets (including its `listening_socket_`); then allow them to send/receive data into/out from their `inbound_data_` and `outbound_data_` buffers; and finally, use the `OrderServer::outgoing_responses_` pointer to access each MEClientResponse from the `matching_engine` and send the responses to the respective clients.

However, there are no `OrderServer::outgoing_responses_` to be read, nor clients in this example, so there is no comment coming from the `tcp_server_` or the `OrderServer::listener_socket_` neither at `/home/cod-salmon/low-latency-app/exchange/outputs/exchange_order_server.log`. Instead, we just see
```
/usr/src/exchange/order_server/order_server.h:27 run() Mon Jan 13 00:35:29 2025
``` 
