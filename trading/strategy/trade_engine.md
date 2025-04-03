# The TradeEngine
A TradeEngine consists of four main components:
- a FeatureEngine instance, `feature_engine_`, holding the fair market price, `mkt_price_` and aggregate trade quantity ratio, `agg_trade_qty_ratio_`, together with methods to access these variables and to update these variables as the trade engine keeps lives on;
- a PositionKeeper instance, `position_keeper_`; which keeps track of the current trading position (buy or sell) and the total PnL.
- an OrderManager instance, `order_manager_`, which processes the trading decisions from the trading algorithms (order requests) and passes them to the order gateway client to be sent to the exchange; and also processes the responses from the exchange coming from the order gateway client
- a RiskManager instance, `risk_manager_`, which evaluates the risk taken with some trading decision and the risk taken already by trading strategies. 
together with two links to two different types of algorithms:
- a MarketMaker pointer, `mm_algo_`, and
- a LiquidityTaker pointer, `taker_algo_`; which are the two opposite strategies implemented in this code. While the first will look to create liquidity on the market, the latter will remove that liquidity. 
- finally, links to three different queues:
- a `Exchange::ClientRequestLFQueue` pointer, `outgoing_ogw_requests_`;
- a `Exchange::ClientResponseLFQueue` pointer, `incoming_ogw_responses_`; and
- a `Exchange::MEMarketUpdateLFQueue` pointer, `incoming_md_updates_`.

## `TradeEngine::TradeEngine`
It builds a TradeEngine instance by:
1. Initialising the `client_id`, setting `outgoing_ogw_requests_` to point the input `client_requests`; setting the `incoming_ogw_responses_` to points to the input `client_responses`; setting the `incoming_md_updates_` to the input `market_updates`; and
2. Initialising its internal `feature_engine`, `position_keeper_`, `order_manager_` and `risk_manager_`.
3. Allocating a new MarketOrderBook per MarketOrderBook space in the `ticker_order_book_`, and setting the trading engine of each of these MarketOrderBooks to the current engine.
4. Setting up the functions to call when needed to do a callback for `OnBookUpdate`, `OnTradeUpdate` and `OnOrderUpdate`; and
5. Initialising the respective (`mm_algo_` or `taker_algo_`) algorithm, according to the input `algo_type`.

## `TradeEngine::sendClientRequest`
This method takes an MEClientRequest type and writes it down on `outgoing_ogw_requests_` to send to the exchange.

## `TradeEngine::run`
This method loops over all `incoming_ogw_responses_` and calls `onOrderUpdate` on each of them thus updating the position keeper and informing the trading algorithm about the response.
Then it loops over the `incoming_md_updates` and updates the market order book corresponding to the update's `ticker_id`.

## `TradeEngine::onOrderBookUpdate`
When there's an update on the MarketOrderBook, then the best-bid-offer or (BBO) may change. This method takes a pointer to an updated MarkerOrderBook, and from it it extracts the BBO, which then passes onto the `position_keeper_`, the `feature_engine_` and informs about to the trading algorithms.

## `TradeEngine::onTradeUpdate`
When a trade event occurs, this method informs the `feature_engine` as well as the trading algorithms about the trade event.

## `TradeEngine::onOrderUpdate`
Processes responses from the exchange, updating the `position_keeper_` and informing the trading algorithms about the response.




