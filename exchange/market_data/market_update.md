# Market Update
In the `market_update.h` file we have the following:
- A **market update**: A definition of the `MEMarketUpdate` struct. Every instance of this struct will hold specific values for some typedefs from `common/types.h`. 
- A **to-string method for the market update**. The `MEMarketUpdate` struct has a `toString`  method to print itself (the `common/types.h` values that characterise it).
- A **market update type**. An enum class called `MarketUpdateType` which can be either: 
    - ADD - to inform the market that a new order has been added;
    - MODIFY - to inform the market that an existing order has been modified;
    - CANCEL - to inform the market that an existing order has been cancelled;
    - TRADE - to inform the market that a trade event has happened;
    - INVALID
- A **to-string method for the client request type**. 

We also define a new type of container called `MEMarketUpdateLFQueue`, which is an `LFQueue` (see `common/lf_queue.h`) holding `MEMarketUpdate` objects.

