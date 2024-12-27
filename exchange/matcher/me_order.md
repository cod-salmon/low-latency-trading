At `me_order.h` we declare:

# The MEOrder struct
Another struct composed of a:
- `ticker_id_`,
- `client_id_`, to identify the client the order is respective to;
- `client_order_id_`, which identifies the client's order when arriving from the Order Gateway Client;
- `market_order_id_`, which identifies the client's order when sent to the Market Data Publisher;
- `side_`, which tells whether we the order is a buy/sell order;
- `price_`, of this order;
- `qty_`, the amount the client wants to buy/sell for this `price_`;
- `priority_`, which represents the position of this order in the queue of other MEOrder instances with the same `side_` and `price_` values in the OrderBook. Note the position in the queue gets assigned on a FIFO basis. 
- two `MEOrder` pointers to the `prev_order_` and `next_order_` in the MEOrdersAtPrice struture (see just below).

# The MEOrdersAtPrice struct
The `MEOrdersAtPrice` struct keeps all orders with a specific `side_` and `price_`. The first order in the queue is saved at the `first_me_order`. It also has two `MEOrdersAtPrice` pointers, `prev_entry_` and `next_entry_` for the previous and next `MEOrdersAtPrice` entries in the list from the most aggressive to the least aggressive prices on the buy and sell sides.

# Three typedefs
- `OrderHashMap`, an array of `MEOrder` pointers;
- `ClientOrderHashMap`, an array of `OrderHashMap` objects; and
- `OrdersAtPriceHashMap`, an array of `MEOrdersAtPrice` pointers.

Note we say hashmap instead of array in these typedefs as we are using the position of the objects in the array as the key to access them specifically. 

Note also that, unlike for `OrderHashMap` and `ClientOrderHashMap`, where the `order_id` and `client_id` are used as indeces for the array, for obtaining the index for a given `MEOrdersAtPrice` object in a `OrdersAtPriceHashMap`, we take it's `price_` and do the mod: `(price_)mod(ME_MAX_PRICE_LEVELS)` (see, for example, the `priceToIndex` method in `exchange/me_order_book.h`).


