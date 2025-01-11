# The SnapshotSynthesizer
When `SnapshotSynthesizer::start` is called, `SnapshotSynthesizer` starts to process `SnapshotSynthesizer::outgoing_md_updates_` through `SnapshotSynthesizer::run`. Only when `SnapshotSynthesizer::stop` gets called, `SnapshotSynthesizer::run_` is set to false and the `SnapshotSynthesizer` will stop processing `SnapshotSynthesizer::snapshot_md_updates_`. 

During `SnapshotSynthesizer::run` then, each `MDPMarketUpdate` from `snapshot_md_updates_` (which is, again, getting continuously updated by the `MarketDataPublisher`), gets added to the snapshot through `SnapshotSynthesizer::addToSnapshot`. Evertime the loop over `snapshot_md_updates_` is finished, we call `SnapshotSynthesizer::publishSnapshot`, to show whatever updates we got to the market.

## `SnapshotSynthesizer::addToSnapshot`
Called during the loop over `SnapshotSynthesizer::snapshot_md_updates_`, during `SnapshotSynthesizer::run`. Takes a MDPMarketUpdate. We:
1. extract the MEMarketUpdate from the input MDPMarketUpdate, and the `orders` that we are going to affect with that MEMarketUpdate from our `ticker_orders` (using the MEMarketUpdate `ticker_id_`). If:
    - the MEMarketUpdate is of type ADD, then we place the MEMarketUpdate order to the existing `orders` that we have in its corresponding `order_id_`; else if
    - the MEMarketUpdate is of type MODIFY, then we take the specific `order` with `order_id_` and modify its `qty_` and `price_` according to the MEMarketUpdate; else if
    - the MEMarketUpdate is of type CANCEL, then we take the `order` with `order_id_` from `orders`, deallocate any memory assigned to it, and set its place at the `order_id_` index inside `orders` to `nullptr`.
2. extract the `seq_num_` from the MDPMarketUpdate, check that this matches with `last_inc_seq_num_` + 1, and if so, update `last_inc_seq_num_` with the `seq_num_`.

## `SnapshotSynthesizer::publishSnapshot`
1. We set `snapshot_size` equal to zero.
2. We create a MDPMarketUpdate of type SNAPSHOT_START, called `start_market_update`, and with `seq_num` equal to `last_inc_seq_num_`. Then we send the MDPMarketUpdate to the `snapshot_socket_` to stream it to the market participants.
3. For each `ticker_id`, we generate a MDPMarketUpdate of type CLEAR, called `clear_market_update`, instructing the market to clear their books for their respective `ticker_id` (e.g., type of stock/market/etc.). Its `seq_num` is equal to `snapshot_size++` (so 1, for the first iteration). Then we send the MDPMarketUpdate to the `snapshot_socket_` to stream it to the market participants.
4. Once the previous snapshot for the current `ticker_id` is cleared, we loop over every `order` in the `orders` we got for that `ticker_id`. Note `orders` is a fixed-size array. Each index (`order_id`) gets some MEOrder memory assigned to it. If no order was ever placed on (or otherwise deallocated from) the memory for `order_id`, then it will be nullptr and there is nothing to do with it; otherwise, we generate a MDPMarketUpdate of type ADD for that `order`, assign it a `seq_num` equal to `snapshot_size++` (so 2, 3, 4, etc. for the first iteration) and send it to the `snapshot_socket_`. This will place the `order` we got for the snapshot in the market participants current snapshot.
5. Once finished publishing all `orders` for the current `ticker_id`, we create a MDPMarketUpdate of type SNAPSHOT_END, called `end_market_update`, and with `seq_num` equal to `last_inc_seq_num_` (same as with `start_market_update`). We send the MDPMarketUpdate to the `snapshot_socket_` to stream it to the market participants. Note that we input `last_inc_seq_num_` in both `start_market_update` and `end_market_update` as the `OrderId` value in their MEMarketUpdate, so that the user can match the snapshot market data stream with the incremental market data stream.

