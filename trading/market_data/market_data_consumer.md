# The MarketDataConsumer
The first component we need to build inside the market participants' trading system is the market data consumer. This component is responsible for 
- subscribing to the multicast stream of public market data updates published by the trading exchange; 
- detecting packet drops on the incremental market data stream; and 
- providing mechanisms to recover and synchronize with the market data stream again.

Inside the MarketDataConsumer class we define a new typdef called `QueuedMarketUpdates`. This is a std::map `seq_num_` to `Exchange::MEMarketUpdate`. Recall that std::map is an ordered container, and orders its elements by key (`seq_num_`). Both `snapshot_queued_msgs_` and `incremental_queued_msgs_` are `QueuedMarketUpdates`. They get populated during `MarketDataConsumer::queueMessage`.

### `MarketDataConsumer::queueMessage`
It adds the input `request` to either `snapshot_queued_msgs_` or `incremental_queued_msgs_` according to the value of `is_snapshot`. At the end of it, we call `MarketDataConsumer::checkSnapshotSync`. This might clear either `snapshot_queued_msgs_` or both `snapshot_queued_msgs_` and `incremental_queued_msgs_`  back to zero, in an attempt to sync both streams.

### `MarketDataConsumer::checkSnapshotSync`
Checks `snapshot_queued_msgs_`: 
- is our first element in `snapshot_queued_msgs_`, `first_snapshot_msg`'s type, SNAPSHOT_START? If not, our snapshot is incomplete so we clear `snapshot_queued_msgs_` and do nothing; otherwise we loop over the remaining updates from `snapshot_queued_msgs_`:
- if the sequence number associated with the update does not match the expected snapshot sequence (e.g., the MEMarketUpdate from `snapshot_queued_msgs_` is mapped to a `seq_num_` = 5, but our expected `next_snapshot_seq` = 3 - meaning, we missed the updates for `seq_num_` = 3 and `seq_num_` = 4), then we set the snapshot as incomplete, by setting `have_complete_snapshot` to false, break the loop, clear `snapshot_queued_msgs_` and return. Otherwise, we continue until we finish with all `snapshot_queued_msgs_` (except for SNAPSHOT_END).
- if the last update in `snapshot_queued_msgs_`, `last_snapshot_msg`'s type, is SNAPSHOT_END, then the snapshot is complete and we are allowed to continue down `MarketDataConsumer::checkSnapshotSync`. Otherwise, we don't clear `snapshot_queued_msgs_`, but we return from `MarketDataConsumer::checkSnapshotSync`, hoping that the next request processed by `MarketDataConsumer::recvCallback` will be of type SNAPSHOT_END and complete the snapshot.

Say `snapshot_queued_msgs_`. We take `last_snapshot_msg`, and set our next expected incremental sequencial number (or `next_exp_inc_seq_num_`) to the last snapshots message's `order_id_` + 1. Now we loop over `incremental_queued_msgs_`:
- if the current incremental message `seq_num_` is less than `next_exp_inc_seq_num_`, then we can play catch-up by letting the loop run until we hit `next_exp_inc_seq_num_`. Otherwise, 
- if the current incremental message `seq_num_` is not less than `next_exp_inc_seq_num_`, neither equal to `next_exp_inc_seq_num_`, then we have missed some update(s) in the incremental stream, so `have_complete_incremental` becomes false, we break the loop and clear `snapshot_queued_msgs_`. Otherwise, we continue pushing updates into `final_events` (except for SNAPSHOT_START and SNAPSHOT_END).

If we managed to finish looping over `snapshot_queued_msgs_` successfully:
- we copy `final_events` onto `incoming_md_updates_`, which will be immediately processed by the trading engine.
- we clear both `snapshot_queued_msgs_` and `incremental_queued_msgs_`; and
- we unsubscribe from the incremental stream.

### `MarketDataConsumer::MarketDataConsumer`
During construction of the MarketDataConsumer, both `snapshot_mcast_socket_` and `incremental_mcast_socket_` get setup. While `incremental_mcast_socket_` joins the multicast stream at this point, `snapshot_mcast_socket_` joins the multicast stream just through `MarketDataConsumer::startSnapshotSync`, which gets called only if a drop gets detected in the incremental stream during `MarketDataConsumer::recvCallback`. Joining the multicast stream allows the sockets to gather market data updates published by the trading exchange. 

### `MarketDataConsumer::run`
During `MarketDataConsumer::run`, `McastSocket::sendAndRecv` gets called from both the `incremental_mcast_socket_` and the `snapshot_mcast_socket_`. Recall that `McastSocket::sendAndRecv` does two things: reads in data into the current socket's `inbound_data_` buffer and reads out data from the current socket's `outbound_data_` to the socket it is connected to. When data is received into either the `incremental_mcast_socket_`'s or the `snapshot_mcast_socket_`'s  `inbound_data_`, `MarketDataConsumer::recvCallback` gets called.

### `MarketDataConsumer::recvCallback`
When `MarketDataConsumer::recvCallback` gets called from `snapshot_mcast_socket_`, there are two options. Either:
- we are not `in_recovery_` (there was no gap yet detected on the incremental stream), and so there is nothing to do from the `snapshot_mcast_socket_` side; or
- we are `in_recovery_`, so we loop over `inbound_data_` by chunks the size of a `Exchange::MDPMarketUpdate`. For each of those data chunks, we cast them into a `Exchange::MDPMarketUpdate` struct, and keep it under `request`. Now, if this is the first iteration where we are `in_recovery_` (so `in_recovery_` is true but `alredy_in_recovery` is false), then we start by calling `MarketDataConsumer::startSnapshotSync`, which resets `snapshot_queued_msgs_` and `incremental_queued_msgs_` and sets up `snapshot_mcast_socket_`, by subscribing it to the snapshot multicast stream. In any case, we then call `MarketDataConsumer::queueMessage`, which adds the `request` to both `snapshot_queued_msgs_` and `incremental_queued_msgs_` and then calls `MarketDataConsumer::checkSnapshotSync`, which checks both maps and attempts to sync them. If it manages to sync them, then it sets `in_recovery_` to false. 

The way `MarketDataConsumer::checkSnapshotSync` syncs snapshot to incremental stream (see above) is (1) by checking that `snapshot_queued_msgs_` is a complete snapshot; then (2) if the `incremental_queued_msgs_` lag behind `snapshot_queued_msgs_`, let them run in the loop until catching up with the snapshot. If a gap gets detected in `incremental_queued_msgs_` when catching up, then a message gets printed to inform of our inability to sync both streams; we clear `snapshot_queued_msgs_`, and we exit `MarketDataConsumer::checkSnapshotSync`. If, on the other hand, it was possible to sync both streams, then the full `incremental_queued_msgs_` is passed onto `incoming_md_updates_` to be processed by the trading engine; we clear both `snapshot_queued_msgs_` and `incremental_queued_msgs_`, and unsubscribe from the incremental stream (until a new gap gets detected on the `inbound_data_` during `MarketDataConsumer::recvCallback`).
