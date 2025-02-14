# The RiskCfg
This is a struct holding the
- `max_order_size_`, the maximum allowed order 
size that a strategy is allowed to send; the 
- `max_position_`, the maximum position 
that a strategy is allowed to build; and the
- `max_loss_`, the maximum allowed loss before the 
trading strategy is shut off from trading further.

# The TradeEngineCfg
This struct encapsulates the trade engine configurations, holding the 
- `clip_`, the "packet size", what the trading strategies will use as the quantity units in which to send out their orders; the
- `threshold_`, used by the trading strategies against the feature values to decide if a trading decision needs to be made or not; and the
- `risk_cfg_`, which contains values to compare against and decide whether an order's quantity is too large (`RiskCheckResult::ORDER_TOO_LARGE`), an order's position is too large (`RiskCheckResult::POSITION_TOO_LARGE`) or the total loss is too large (`RiskCheckResult::LOSS_TOO_LARGE`). This is done through `RiskInfo::checkPreTradeRisk`, called through `RiskManager::checkPreTradeRisk`.

## The TradeEngineCfgHashMap
This is an array of `TradeEngineCfg` for each `ticker_id_`.

# The RiskInfo struct
This struct is used to assess the risk of certain trade, through `RiskInfo::checkPreTradeRisk`. Thus, it contains a `position_info_` and a `risk_cfg_` that uses:
(a) to obtain the `position_` and compare against the `risk_cfg_`'s `max_position_` to assess whether the position is too large;  
(b) to obtain the `total_pnl_` and compare against the `risk_cfg_`'s `max_loss_` to assess whether the loss is too large; and 
(c) to compare the `risk_cfg_`'s `max_order_size_` to the `qty` the trading strategy is considering to trade.

## The TickerRiskInfoHashMap
This is an array of `RiskInfo` structs, one for each `ticker_id_`.

# The RiskManager class
The RiskManager class is a class consisting of two main components:
- a TickerRiskInfoHashMap instance, `ticker_risk_`, mapping `RiskInfo` struct to `ticker_id_` and used by
- the `RiskManager::checkPreTradeRisk` method to assess the risk of a specific trade; this one consisting on buying/selling (depending on `side`) some `qty` of a stock with ID `ticker_id_`. 

