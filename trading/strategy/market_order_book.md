# The `MarketOrderBook` class
Keeps all orders within a specific `ticker_id_` (stock, market, etc.). A `MarketOrderBook` will have 
- its own pointer to the strategy's `trade_engine`, which uses to communicate its passive orders and get updated as different trade events happen for the `ticker_id_` stock. 
- its own OrderHashMap instance, `oid_to_order_`, mapping `order_id` to `order`. This allows the `MarketOrderBook` to update its book as it receives order updates from the market data publisher.
- a memory pool for MarketOrder objects `order_pool_`, to keep all orders for the `ticker_id_` stock. Orders are organised in containers. Orders with the same `side_` and `price_` are assigned to the same `MarketOrdersAtPrice` container, queued up in FIFO order. 
- the containers themselves also need some memory to be assigned to, so we have an extra memory pool for the `MarketOrdersAtPrice` objects. A `MarketOrdersAtPrice` object will inheritely be positioned in a queue, in order from the most aggressive to the least aggressive price within all existing `MarketOrdersAtPrice`;
- a `bids_by_price_`, which points to the best bid for the `MarketOrderBook`'s `ticker_id_` stock;
- a `asks_by_price_`, which points to the best ask for the `MarketOrderBook`'s `ticker_id_` stock;
- a `price_orders_at_price_`, which maps price level to `MarketOrdersAtPrice` object in the `orders_at_price_pool_` memory pool. To obtain the index from a given price, we use `MarketOrderBook::priceToIndex`, and to obtain the `MarketOrdersAtPrice` for a given price index (price), we use `MarketOrdersAtPrice::getOrdersAtPrice`;
- a `bbo_`, to keep the best bid (`bid_qty_` at `bid_price_`) and ask (`ask_qty_` at `ask_price_`).

## `MarketOrdersAtPrice::addOrdersAtPrice`
Takes a queue of new orders sharing the same price, `new_orders_at_price`. First, it places the new orders at the specified price index (`priceToIndex`) inside the `price_orders_at_price`. Then we adjust `bids_by_price_`/`asks_by_price_` and the internal list the offers follow accordingly.

- On the SELL side, `asks_by_price_` represents the best queue of SELL orders; while on the BUY side, `bids_by_price_` represents the best queue of BUY orders. According to the side of `new_orders_at_price`, either one or the other is selected as the `best_orders_by_price`.
- It is possible that the `best_orders_by_price` (`bids_by_price_` or `asks_by_price_`) are yet empty. In that case, we set `bids_by_price_` (or `asks_by_price_`) equal to `new_orders_at_price` and its `prev_entry_` and `next_entry_` equal to itself.
- Otherwise, `best_orders_by_price` becomes our `target` (the reference point which we will place `new_orders_at_price` either before or after):
    - If the new orders are on the SELL side, and if the price at which they are wished to be sold is greater than the price at which the target orders are wished to be sold; OR, if the new orders are on the BUY side, and if the price at which they are wished to be bought is less than the price at which the target orders are wished to be bought, then `add_after = true` and we add the new orders after the `target` (as they are being less aggressive than the target price);
    - Recall that a `MEOrdersAtPrice` struct keeps all orders with a specific `side_` and `price_`. It also has two `MEOrdersAtPrice` pointers, `prev_entry_` and `next_entry_` for the previous (more aggressive) and next (less aggressive) `MEOrdersAtPrice` entries in the list it belongs to, and which goes from the most aggressive to the least aggressive prices on the buy and sell sides. Therefore, we start with `target = best_orders_by_price` (which can be either `bids_by_price_` or `asks_by_price_`), and by calling `target = target->next_entry_`, we are moving downwards in aggressiveness.
    - Therefore, to know where exactly to add `new_orders_at_price_`, we keep evaluating 
    ```
    add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                 (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));

    ```
    and `target = target->next_entry_` (moving the reference downwards), and comparing offers until we either (a) reach a point where the new orders are more aggressive than the current target (`add_after` becomes false); or (b) we hit `best_orders_by_price` again. 
    - Following the comments on `exchange/matcher/me_order_book.h`:
        - CASE A. First time we evaluate `add_after` and was found to be `false`. This means `new_orders_at_price` have a better offer than the current `best_orders_by_price`. Therefore:
            (1) We will place `new_orders_at_price` before `target` (which in this case is `best_orders_by_price`). 
            (2) We need to check whether `target` was the last element in the queue. In that case, its `next_entry_` would be pointing to `best_orders_by_price` and we would need to update it to `new_orders_at_price`.
            (3) As `new_orders_at_price` becomes the best bid/ask, we need to update `bids_by_price_`/`asks_by_price_`.
        - CASE B. Second time we evaluate `add_after` and now was found to be `false`. This means `new_orders_at_price` has the second best offer after `best_orders_by_price`. Therefore:
            (1) We will place `new_orders_at_price` before `target` (which in this case was below `best_orders_by_price`). 
            (2) We don't need step (2) as in CASE A, because `target` needs to still be pointing to `best_orders_by_price`.
            (3) We don't need step (3) as in CASE A, because `new_orders_at_price` is not the best bid/ask in the queue.
        - CASE C. Third/fourth/fifth, etc. time we evaluate `add_after` and finally was found to be `false`. It runs like CASE B except for when we hit the end of cycle and `target` comes back to being equal to `best_orders_by_price`. That special case is CASE D.
        - CASE D. `new_orders_at_price` is worse than any offer in the queue, so we add it at the end.
    - Hence, in CASE A, B and C:
        `target->prev_entry_` ---- `new_orders_at_price` ---- `target`
    and in CASE D:
        `target` ---- `new_orders_at_price` ---- `target->next_entry_`

## `MarketOrderBook::addOrder`
Uses the `order`'s `price_` to obtain all `orders_at_price_` from `price_orders_at_price_`. 
- If this is empty, then create a new MarketOrdersAtPrice (i.e., allocate it to some memory in the `orders_at_price_pool_`)  called `new_orders_at_price`  (consisting only of the input `order`) and add it to `price_orders_at_price_`.
- If it is not empty, add order following FIFO (i.e., on top of the `orders_at_price_`'s `first_mkt_order_`, as this is the last element in the circular queue starting from `first_mkt_order_`). 
- In any case, add `order` to the `oid_to_order_` hashmap.

## `MarketOrderBook::removeOrdersAtPrice`
The reverse method to `addOrdersByPrice`. We get `orders_at_price_` for the given `side` and `price`. We also get the `best_orders_by_price` (`bids_by_price`/`asks_by_price`) according to the input (buy/sell) `side`. 
- If it happens that `orders_at_price_` is the only entry in the buy/sell book, then we need to set `bids_by_price`/`asks_by_price` to `nullptr`, as there is no best bid/ask once we remove that entry.
- If not, (1) remove link to this `MEOrderAtPrice` from its `previous_` and `next_` entries; (2) if `orders_at_price` turns out to be also the best bid/ask (or equal to `best_orders_by_price`), then we need to shift `bids_by_price`/`asks_by_price` to `orders_at_price`'s `next_entry_`; and (3) we set `orders_at_price`'s previous and next entries to null.
- Finally, we set the `MEOrderAtPrice` pointer for the input `price` and `side` to `nullptr` and deallocate it from its memory in the `orders_at_price_pool_`.

## `MarketOrderBook::updateBBO`
- if `update_bid`, update `bbo_`'s `bid_price_` and `bid_qty_` with the values from `bids_by_price_`. If `bids_by_price_`, then invalidate both `bid_price_` and `bid_qty_`.
- if `update_ask`, update `bbo_`'s `ask_price_` and `ask_qty_` with the values from `asks_by_price_`. If `asks_by_price_`, then invalidate both `ask_price_` and `ask_qty_`.

## `MarketOrderBook::onMarketUpdate`
If:
- `MarketUpdateType::ADD`, `MemPool::allocate` will return a pointer to the MEOrder object in the `order_pool_`, instantiated with the data from the input `market_update`; then we call `MarketOrderBook::addOrder`, to add `order` to `oid_to_order_` and `price_orders_at_price_`.
- `MarketUpdateType::MODIFY`, we obtain `order` from `oid_to_order_`, given the input `market_update`'s `order_id_`; then update the `order`'s `qty_` to the one given in the `market_update_`.
- `Exchange::MarketUpdateType::CANCEL`, we obtain `order` from `oid_to_order_`, given the input `market_update`'s `order_id_`; then call `MarketOrderBook::removeOrder`, to remove the `order` from `oid_to_order_`, `order_pool_` and `price_orders_at_price_`.
- `Exchange::MarketUpdateType::TRADE`, we simply forward the message down to the `trade_engine_`, via `TradeEngine::onTradeUpdate` to update its `agg_trade_qty_ratio_`, and as trade messages do not modify the market order book, we return afterwards.
- `Exchange::MarketUpdateType::CLEAR`, we deallocate all assigned data on `order_pool_`, we reset all pointers in `oid_to_order_`, and deallocate all memory used in `orders_at_price_pool_`-

If it happens that the `market_update` brings a better bid/ask than the current best bid/ask, then we will update the `bbo_` accordingly through `MarketOrderBook::updateBBO`.

If we have not yet returned by the end of the switch statement, that means that some update happened on the market order book, so we call `TradeEngine::onOrderBookUpdate` to update the `trade_engine_`'s `mkt_price_`.