# The Market Data Publisher
There are two flows of information in the MarketDataPublisher which happen in parallel. Say some updates occurred on the order book. The matching engine informs the MarketDataPublisher of such updates through a queue of `MEMarketUpdate` objects called `outgoing_md_updates_`. Then, each `MEMarketUpdate` in `outgoing_md_updates_`:
- gets directly published to the UDP multicast stream; and
- wrapped into a `MDPMarketUpdate` struct and added to the `snapshot_md_updates_` queue to be processed by the `snapshot_synthesizer_`.

## `MDPMarketUpdate` and `MDPMarketUpdateLFQueue`
While the matching engine communicates updates to the MarketDataPublisher through `MEMarketUpdate` structs, the MarketDataPublisher communicates updates to the market through `MDPMarketUpdate` structs. The difference between both is that the `MDPMarketUpdate` works as a wrapper on the `MEMarketUpdate` holding an additional `seq_num_`. This is the sequence number field. It is used by both the market and the MarketDataPublisher to check for sync failures. In addition to the new `MDPMarketUpdate`, there is a new typedef `MDPMarketUpdateLFQueue`, an LFQueue of `MDPMarketUpdate` objects, used by the MarketDataPublisher to process the updates.

## The MarketDataPublisher
Therefore, the `MarketDataPublisher` keeps:
- an `incremental_socket_`, which is `McastSocket` (see `common/mcast_socket.hpp`) to be used to publish UDP messages on the incremental multicast stream;
- A `MEMarketUpdateLFQueue` pointer, called `outgoing_md_updates_`, which points to the `market_updates` from the matching engine;
- A `MDPMarketUpdateLFQueue` instance, called `snapshot_md_updates_`, where updates from the matching engine are wrapped into a MDPMarketUpdate struct, and passed onto the `snapshot_synthesizer_`. 
- A `snapshot_synthesizer_`, which processes `snapshot_md_updates_` as these get updated in the `MarketDataPublisher`.

### `MarketDataPublisher::run`
As `run_` keeps true, `MarketDataPublisher::run` will keep looping over the `outgoing_md_updates_`, which are continuously updated by the matching engine, and:
1. Send each of those updates, together with their corresponding sequence number, to the `incremental_socket_`, for multicast streaming; and
2. Write each of those updates to the `snapshot_md_updates_`, which then get immediately processed by the `snapshot_synthesizer_`.

### `MarketDataPublisher::start` and `MarketDataPublisher::stop`
When `MarketDataPublisher::start` gets called, `MarketDataPublisher::run` is set to true, starting the continuous loop over `MarketDataPublisher::outgoing_md_updates_` in `MarketDataPublisher::run`. It will also call `SnapshotSynthesizer::start`, which in a similar manner, will set `SnapshotSynthesizer::run` to true and start the continuous loop over `SnapshotSynthesizer::snapshot_md_updates_` (which carry the same `MarketDataPublisher::outgoing_md_updates_`, but wrapped on a `MDPMarketUpdate` struct, instead of `MEMarketUpdate`). 

When `MarketDataPublisher::stop` is called, `MarketDataPublisher::run` and `SnapshotSynthesizer::run` (through `SnapshotSynthesizer::stop`) are set to false, stopping any more processing of `MarketDataPublisher::outgoing_md_updates_` and consequently `SnapshotSynthesizer::outgoing_md_updates_`.

#### How `MDPMarketUpdates` are added to a snapshot
When the `MarketDataPublisher` constructor gets called, its internal `snapshot_synthesizer_` gets instantiated and `SnapshotSynthesizer::snapshot_synthesizer_` gets referenced to `MarketDataPublisher::snapshot_md_updates_`, which at that point (just when the `MarketDataPublisher` gets created) is obviously empty. However, once `MarketDataPublisher::start` gets called, this will trigger `SnapshotSynthesizer::start`, which will set `snapshot_synthesizer_`'s `run_` to true and activate a constant loop over `SnapshotSynthesizer::snapshot_md_updates_`. Because this points to `MarketDataPublisher::snapshot_md_updates_`, as `MarketDataPublisher::snapshot_md_updates_` gets updated during `MarketDataPublisher::run`, so do `SnapshotSynthesizer::snapshot_md_updates_`. As new `MDPMarketUpdate`s get added to `SnapshotSynthesizer::snapshot_md_updates_`, they are then added to the snapshot through `SnapshotSynthesizer::addToSnapshot`.