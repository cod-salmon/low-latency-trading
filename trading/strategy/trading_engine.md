# The TradeEngine
A TradeEngine consists of four main components:
- a FeatureEngine instance, `feature_engine_`, holding the fair market price, `mkt_price_` and aggregate trade quantity ratio, `agg_trade_qty_ratio_`, together with methods to access these variables and to update these variables as the trade engine keeps lives on;
- a PositionKeeper instance, `position_keeper_`;
- an OrderManager instance, `order_manager_`, and
- a RiskManager instance, `risk_manager_`; 
together with two links to two different types of algorithms:
- a MarketMaker pointer, `mm_algo_`, and
- a LiquidityTaker pointer, `taker_algo_`;
as well as links to three different queues:
- a `Exchange::ClientRequestLFQueue` pointer, `outgoing_ogw_requests_`;
- a `Exchange::ClientResponseLFQueue` pointer, `incoming_ogw_responses_`; and
- a `Exchange::MEMarketUpdateLFQueue` pointer, `incoming_md_updates_`.


