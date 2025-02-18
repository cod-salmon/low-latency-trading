# The MarketMaker class
This contains:
- a link to a `feature_engine_`, that drives the market-making algorithm;
- a link to an `order_manager_`, for the market-making algortihm to manage its passive orders;
- an array, `ticker_cfg_`, mapping each `ticker_id_` to a TradeEngineCfg.

## `MarketMaker::onOrderBookUpdate`
Recall that a BBO is a struct holding the best bid and ask passive orders (`bid_qty_` at `bid_price_` and `ask_qty_` at `ask_price_`). When calling `MarketMaker::onOrderBookUpdate`, we 
- first fetch the BBO from the input MarketOrderBook `book`. 
- Secondly, we fetch the market price, which is a `double` computed from the `bbo`'s `bid_qty_`, `bid_price_`, `ask_qty_` and `ask_price_`. It is computed in the `feature_engine_` as the book quantity weighted price or (`bid_price_` * `ask_qty_` + `ask_price_` * `bid_qty_`) / 
(`bid_qty_` + `ask_qty_`).
- Then we fetch the `threshold_` and `clip_` from the `ticker_cfg_` for the specific `ticker_id_`. Note that `clip_` sets the quantity amount that the trading strategy will send out (e.g., 100 stocks); and `threshold_` sets the minimum difference we want between the fair or market price and the `bbo`'s bid/ask price. When the difference between 
- the fair price and the `bbo`'s bid price is equal or more than `threshold_`, this means that the market is willing to buy at this or more than the current `bid_price_`. Therefore we should buy at `bid_price_`, expecting prices to go higher. Otherwise, we are not so confident that prices will go higher later, so we set `bid_price` to be equal to `bid_price_` minus one unit.
- the `bbo`'s ask price and the fair price is equal or more than `threshold_`, this means that the market is asking for a lower price than the current `bbo`'s `ask_price_`, so we should sell at this price and expect prices to go lower. Otherwise, we are not so confident that prices will go lower later, so we set `ask_price` to be equal to `ask_price_` minus one unit.

Then we call `OrderManager::moveOrder` on the `bbo`'s bid order for the respective `ticker_id`, for an amount `clip` at `bid_price`; and similarly, we call `OrderManager::moveOrder` on the `bbo`'s ask order for the respective `ticker_id`, for an amount `clip` at `ask_price`. Note that `OrderManager::moveOrder` will either send a cancel request to the exchange (for the `clip` amount at the bid/ask price), or a new order request (for the `clip` amount at the bid/ask price), or simply don't do anything on the order if no change in the bid/ask price is made. The responses back from the exchange are processed during `MarketMaker::onOrderUpdate`, which internally calls `OrderManager::onOrderUpdate`.

## `MarketMaker::onTradeUpdate`
This is to process trade events, which for the market-making algortihm is none. This one just places orders.