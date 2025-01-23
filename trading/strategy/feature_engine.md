# The FeatureEngine class
The FeatureEngine holds two variables, the `mkt_price_` and the `agg_trade_qty_ratio_`. They can be accessed through `FeatureEngine::getMktPrice` and `FeatureEngine::getAggTradeQtyRatio`. They get updated through `FeatureEngine::onOrderBookUpdate` and `FeatureEngine::onTradeUpdate`, respectively.

### `FeatureEngine::onOrderBookUpdate`
The fair market price, or `mkt_price_`, gets computed using the best book order, `bbo_`, as the money from selling `ask_qty_` (the maximum quantity we are willing to sell) by `bid_price_` (the maximum price at which they are willing to buy); plus the money from buying `bid_qty_` (the maximum quantity we are willing to buy) by `ask_price_` (the maximum price at which they are willing to sell), divided by the total buy + sell quantity (`bid_qty_ + ask_price_`).

### `FeatureEngine::onTradeUpdate`
The `agg_trade_qty_ratio_` gets computed as the ratio of the `market_update`'s trade `qty_` to the best bid/ask quantity (given by `bid_qty_`/`ask_qty_`, depending on the `market_update`'s `side_`).