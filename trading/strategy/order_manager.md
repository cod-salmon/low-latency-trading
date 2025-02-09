# The OMOrder struct
The struct encapsulates the orders coming from the OrderManager. For each order we have:
- it's `ticker_id_`, for the type of stock, market, etc.
- it's `order_id_`, which identifies the order;
- it's `side_`, whether it is a buy or a sell;
- the `price_` and `qty_` of the offer; and
- the `order_state_`, which can be INVALID, PENDING_NEW (meaning the order has been sent out by the order manager but it has not yet been accepted by the electronic trading system), LIVE (when the system accepts the offer), PENDING_CANCEL (when the system rejects the offer but has not yet been processed by the exchange) and DEAD (when the order represents an order that does not exist – it has either not been sent yet or fully executed or successfully cancelled).

# Two new types of containers
- a `OMOrderSideHashMap` is a 2D array of OMOrder structs, with one index for the buy side and with one index for the sell side.
- a `OMOrderTickerSideHashMap`, an array of `OMOrderSideHashMap`, ordered by `ticker_id_`.

# The OrderManager class
Keeps track of each pair of best-book-order's ask and bid for every `ticker_id_` (i.e., stock, market, etc.). Contains 
- a link to the `trade_engine_`, allowing the OrderManager to send MEClientRequest objects to the exchange through `TradeEngine::sendClientRequest`.
- a link to the `risk_manager_` that allows it to access the `RiskManager::checkPreTradeRisk` method to assess whether to take or not to take a new order.
- a OMOrderTickerSideHashMap instance called `ticker_side_order_`, that keeps a map of `side_index` to the OMOrder for the specific (buy/sell) side for each of the `ticker_id`s. 

## OrderManager::onOrderUpdate
Takes the MEClientResponse from the exchange and updates its `ticker_side_order_`. If the response from the exchange is:
- `ACCEPTED`, then the `order_state_` from the `order` at `ticker_side_order_.at(client_response->ticker_id_).at(sideToIndex(client_response->side_))` is set to LIVE.
- `CANCELED`, then the `order_state_` from the `order` at 
`ticker_side_order_.at(client_response->ticker_id_).at(sideToIndex(client_response->side_))` is set to DEAD.
- `FILLED`, then first we update the `order`'s `qty_` to be the remaining `qty_` after the order has been filled from the `client_response`. The `order_state_` from the `order` at 
`ticker_side_order_.at(client_response->ticker_id_).at(sideToIndex(client_response->side_))` is set to DEAD if there is no `qty_` left after the update.
- `CANCEL_REJECTED` or `INVALID`, we do nothing as these don't affect the orders.

## OrderManager::newOrder
This is used by the OrderManager to send new order requests to the exchange. First, it generates the request, sends it to the exchange through the `trade_engine_` and then updates the current `order`'s state to PENDING_NEW. Finally, `next_order_id_` is incremented. 

## OrderManager::cancelOrder
This is used by the OrderManager to send cancel order requests to the exchange. First, it generates the request, sends it to the exchange through the `trade_engine_` and finally updates the current `order`'s state to PENDING_CANCEL.

## OrderManager::moveOrder
This is the function wrapping `OrderManager::newOrder` and `OrderManager::cancelOrder` together. 
- If the input `order`'s state is LIVE and the `order`'s `price_` is not equal to the input `price`, then we `cancelOrder`;
- If the input `order`'s state is DEAD, we first make sure that the input `price` is not invalid. Then we check whether we can add the new order in; if so (told by the `risk_manager_`), then we add `newOrder`; otherwise, we simply log out the risk result.

## OrderManager::moveOrders
This is the wrapper for `OrderManager::moveOrder`, used to update the best-book-order's ask and bid orders (when calling  `onOrderBookUpdate` from the MarketMaker or LiquidityTaker).
Therefore, we take `bid_order` and `ask_order` from the `ticker_side_order_` for the input `ticker_id` and we call `OrderManager::moveOrder` on each of them.

**NOTE**: While the general `onOrderUpdate` that you will see in other classes (in here `OrderManager::onOrderUpdate`), is used to update the input `order`'s state, `onOrderBookUpdate` (here you find its lower level format, `OrderManager::moveOrders`) is used to update the order book, and hence not update the `order`'s state but update the order book according to the `order`'s state.