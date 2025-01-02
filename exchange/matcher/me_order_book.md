We declare two objects in `me_order_book.h`:
- a new typedef `OrderBookHashMap`, which is an array of `MEOrderBook` pointers; and
- the `MEOrderBook` class.

# The MEOrderBook class
Some member variables it has:
- a `ticker_id_`, which identifies the stock;
- a `matching_engine_`, which is used by the MEOrderBook instance to send client responses and market updates;
- a ClientOrderHashMap instance called `cid_oid_to_order_`, which is an array of arrays of `MEOrder` pointers, ordered first by `client_id` and then by `order_id`. Therefore, by specifying both as indices, we can access the specific `MEOrder` pointer.
- a `MemPool<MEOrdersAtPrice>` called `orders_at_price_pool_`, where memory to store `MEOrdersAtPrice` objects gets allocated;
- a `MemPool<MEOrder>` called `order_pool_`, where memory to store `MEOrder` objects gets allocated;
- a `MEOrdersAtPrice` pointer called `bids_by_price_`, which is the struct holding the best bids (an array of MEOrders sorted according to FIFO, which share the highest price buyers are willing to pay) from their internal doubly-linked list of `MEOrdersAtPrice`;
- a `MEOrdersAtPrice` pointer called `asks_by_price_`, which is the struct holding the best asks (an array of MEOrders sorted according to FIFO, which share the lowest price sellers are willing to accept) from their internal doubly-linked list of `MEOrdersAtPrice`;
- a `price_orders_at_price_` which is an array of `MEOrdersAtPrice` pointers, sorted by price;
- a `client_response_`, to hold the response from the matching engine to the client; and
- a `market_update_`, to inform the market of any updates on the order book;
- a `next_market_order_id_`, which tracks the next market order_id to be assigned, used in `generateNewMarketOrderId`;
- a `priceToIndex` method, which converts the input price to some index in the `price_orders_at_price_` array; 
- a `getOrdersAtPrice` method, to obtain the ` MEOrdersAtPrice` for a given price.

The main methods for the `MEOrderBook` class are:

## `MEOrderBook::addOrdersByPrice`
Takes a queue of new orders at a speciic price, `new_orders_at_price`. First, it places the new orders at the specified price index (`priceToIndex`)inside the `price_orders_at_price`. Then we adjust `bids_by_price_`/`asks_by_price_` and the internal list the offers follow accordingly.

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

## `MEOrderBook::removeOrdersAtPrice`
The reverse method to `addOrdersByPrice`. We get `orders_at_price_` for the given `side` and `price`. We also get the `best_orders_by_price` (`bids_by_price`/`asks_by_price`) according to the input (buy/sell) `side`. 
- If it happens that `orders_at_price_` is the only entry in the buy/sell book, then we need to set `bids_by_price`/`asks_by_price` to `nullptr`, as there is no best bid/ask once we remove that entry.
- If not, (1) remove link to this `MEOrderAtPrice` from its `previous_` and `next_` entries; (2) if `orders_at_price` turns out to be also the best bid/ask (or equal to `best_orders_by_price`), then we need to shift `bids_by_price`/`asks_by_price` to `orders_at_price`'s `next_entry_`; and (3) we set `orders_at_price`'s previous and next entries to null.
- Finally, we set the `MEOrderAtPrice` pointer for the input `price` and `side` to `nullptr` and deallocate it from its memory in the `orders_at_price_pool_`.

## `MEOrderBook::match`
This function is to try to match an active order (of `client_id` and) of size `leaves_qty` to a passive order (of `client_order_id` and) of size `itr->qty_`. "Active" here refers to some order that has just recently been processed vs. an order which already exists in the order book.

How much the active order gets filled with the existing order is `fill_qty` (the minimum of their quantities). "Matching" consists on reducing both `leaves_qty` and `itr->qty_` by the `fill_qty`. 

Once done, we 
(1) Send a response to both clients whose orders have been matched: to `client_id` we let them know that `fill_qty` has been filled and that `leaves_qty` has been left after the match; to `client_order_id`, we let them know that `fill_qty` has been filled and that `itr->qty_` has been left after the match.
(2) Inform the market that a `fill_qty` has been traded; AND
(3) If there's no more left from `itr` after the matching, then we also inform the market that the order has been cleared; or, if there's still some leftover after the matching, then we also inform the market of the update on `itr`.

## `MEOrderBook::checkForMatch`
Uses `MEOrderBook::match` to get how much would be left over if we were to match `client_id`'s order in the current order book. Note our best offer (most aggressive) order in the order book is kept at `bids_by_price`/`asks_by_price`, which holds a FIFO queue of MEOrder objects (from different clients and different quantities) at price `price`, the first of which is `first_me_order_`. During this method, we loop over the `MEOrders` in `asks_by_price`/`bids_by_price` until either we completely fill the match (`leaves_qty` is zero) or `asks_by_price`/`bids_by_price` becomes `nullptr`. Note the latter can happen when we keep calling `MEOrderBook::match` and matching passive orders without running out of `leaves_qty`. We might keep calling `MEOrderBook::removeOrder` and `MEOrderBook::removeOrdersAtPrice`, updating `asks_by_price`/`bids_by_price` accordingly until there are no more (hence best) asks/bids in the order book.

## `MEOrderBook::add`
To use when we want to add an `MEOrder` to the order book. First thing we do is to generate a market response, informing that we are accepting a new order into the book. Then we call `MEOrderBook::checkForMatch`. This might find a full match (`leaves_qty` becomes zero) between the active order and the existing passive orders, so there is no need to add the order to the list of passive orders. If not, then we assign a priority to the order according to its `price`, add it to the order book (`orders_at_price`), and inform the market that a new order has been added.

## `MEOrderBook::cancel`
Recall that in the `MEOrderBook` we keep two hashmaps:
- `cid_oid_to_order_`, a ClientOrderHashMap, or an array of OrderHashMap objects - that is, an array of arrays of MEOrder pointers, first sorted by `client_id`, then by `client_order_id`; and
- `price_orders_at_price_`, an OrdersAtPriceHashMap, or an array of MEOrdersAtPrice, sorted by price. Recall as well that FIFO gets applied on the MEOrdersAtPrice objects on this array, which are used during matching.

We extract the specific MEOrder for both the `client_id` and `order_id` in `cid_oid_to_order_`. If it's not `nullptr` (there was an order saved there before), then we inform both the client and the market that the order got successfully cancelled and call `MEOrderBook::removeOrder` to remove it from the order book; otherwise, we inform the client that the cancellation got rejected as there is no such order in the order book.

