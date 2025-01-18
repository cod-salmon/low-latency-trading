At `trading/strategy/market_order.h` we declare:
# The `MarketOrder` struct 
This is used to carry the orders the trading engine receives from the MarketDataConsumer.

It holds an `order_id_`, which identifies the order; and a certain `qty_` that the client is willing to buy/sell (specified by `side_`) at a price equal to `price_`.`priority_` represents the exact position of this order in the queue of other MarketOrder instances with the same `side_` and `price_` values in the `MarketOrdersAtPrice` struct.  Then we have `prev_order_` and `next_order_`, which point to the previous and next MarketOrders in that queue. Orders in this list follow FIFO order.

# The `MarketOrdersAtPrice` struct
The MarketOrdersAtPrice struct keeps all orders with a specific `side_` and `price_`. The first order in the queue is saved at the `first_mkt_order_` (to access all remaining orders in the FIFO queue, we would use `first_mkt_order_`'s `next_order_` and `prev_order_` pointers). Just as MarketOrder, MarketOrdersAtPrice also has two `MarketOrdersAtPrice` pointers, `prev_entry_` and `next_entry_` for the previous and next `MarketOrdersAtPrice` entries in the list from the most aggressive to the least aggressive offers on whichever the current `MarketOrdersAtPrice`'s (buy/sell) `side_`is.

# Two typedefs
- `OrderHashMap`, an array of `MarketOrder` pointers, organised by `order_id`;
- `OrdersAtPriceHashMap`, an array of `MarketOrdersAtPrice` pointers, organised by price.

# The BBO struct
The BBO (Best Bid Offer) struct, which holds the current best bid and ask offers: `bid_qty_` at `bid_price_`, and `ask_qty_` at `ask_price_`. 

