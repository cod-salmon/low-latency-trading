# The TradeEngine
The TradeEngine constructor takes:
- a client_id that identifies the specific client
- a ticker configuration. This is a TradeEngineCfgHashMap: an array of TradeEngineCfg objects ordered by ticker_id. Note that a TradeEngineCfg is a struct holding three components: 
    * A `clip_`quantity, which is the size of the orders that the trading strategies send out.
    * A `threshold_` to put against the feature values to decide if a trading decision needs to be made or not.
    * A `risk_cfg_`, which is a RiskCfg object. A RiskCfg holds three components: a max order size, a max position size and the maximum loss that the trading startegy is allowed to have before stopping trading further.

- a queue of client requests that it receives from the order gateway client
- a queue of client responses that it sends back to the order gateway client

Four basic building blocks that will build and support our trading engine:

# Feature Engine
### Which will be used to compute simple and complex features/signals that drive the trading strategy decisions. In this case:
* The onOrderBookUpdate method computes the `mkt_price_`. This is supposed to be called everytime there is an update to the order book. The market price is computed as the book quantity weighted price.
* The onTradeUpdate method computes the `agg_trade_qty_ratio_`. This method is meant to be called everytime there is a trade event in the market data stream. The `agg_trade_qty_ratio_` is the ratio of the trade quantity to the quantity of the BBO that the trade aggresses on.

# Position Keeper
### which will receive executions and compute important measures such as position, PnLs, traded volumes, and more
* `position_`. This is the quantity we are buying/selling.
* `vwap` is the price at which we are willing to buy/sell.  
* `open_vwap` is a two-size array. When Side::BUY, we access `open_vwap[sideToIndex(Side::BUY)]` and update it with (`position_ * vwap_buy`); when Size::SELL, we access `open_vwap[sideToIndex(Side::SELL)]` and update it with (`position_ * vwap_sell`).
* a `ticker_position_` array, which is a hashmap of ticker_id -> PositionInfo object.
* The PositionInfo object keeps track of 
    - the total_pnl_ = real_pnl_ + unreal_pnl_
    - the bbo_
    - the vwap (volume-weighted average price) of opened long/short positions
    - the current long/short position.
Contains two main member functions: 
    - addFill() takes the client response and either makes and execution or not depending on the available positions;
    - updateBBO() updates the bbo_ and updates the unrealised_pnl (an update on the bbo_ should mean an update on the market prices; a change on the market price reflects on the unrealised_pnl).
These functions are the ones called from PositionKeeper for an given ticker_id.

1. Execution: Buy qty = 10 at price = 100.0
position_old_ = position_new_ = 0
position_new_ += qty = 10
open_vwap [0] = price * abs(position_new_) = 1000.0
open_vwap [1] = 0.0
vwap [0] = open_vwap [0] / abs(position_new_) = 100.0
vwap [1] = 0.0
real_pnl = 0.0
unreal_pnl = 0.0 

2. Execution: Buy qty=10 at price=90.0
As we have another buying position, there is no possible match, so no possible realised pnl. We keep all in unrealised.

position_old_ = position_new_ = 10
position_new_ += qty = 20
open_vwap [0] += price * qty += 90 * 10 = 1900
open_vwap [1] = 0.0
vwap [0] = open_vwap [0] / abs(position_new_) = 95.0
vwap [1] = 0.0
real_pnl = 0.0
unreal_pnl = (price - vwap[1]) * position_new_ = -100.0

3. Execution: Sell qty = 10 at price = 92. 
We can sell those, as there is a buying position already registered that is willing to pay even a higher price:

request->price_ (sell side) < vwap[0] (buy side)

so we close qty = 10 out of those 20 positions. We will have some real_pnl coming from closing those selling position, and we will have some remaining unreal_pnl.

As the position_new_ remains positive after this execution, we remain working with the open_vwap[0] and vwap [0].

position_old_ = position_new_ = 20
position_new_ -= qty = 10
open_vwap [0] += 0.0 = 1900.0
open_vwap [1] += 0.0 = 0.0
vwap [0] = open_vwap [0] / abs(position_new_) = 95.0
vwap [1] = 0.0
real_pnl = (price - vwap[0]) * qty = -30.0
unreal_pnl = (price - vwap[0]) * position_new_ = -30.0

4. Execution: Sell qty = 20 at price = 97.
Now position_new_ turns negative, so we set open_vwap[0] and vwap[0] to 0 and focuss on open_vwap[1] and vwap[1]. Note that we had position_old_ = 10 to buy and we are having qty = 20 to sell, so we can only sell (qty - position_old_).

position_old_ = position_new_ = 10
position_new_ -= 20 = -10
open_vwap [0] += 0.0 = 0.0
open_vwap [1] += price * qty = 970.0
vwap [0] = 0.0
vwap [1] = open_vwap [0] / abs(position_new_) = 97
real_pnl += (price - vwap[0]) * (qty - position_old_) = -10.0
unreal_pnl = (price - vwap[1]) * position_new_ = 0.0

5. Execution: Sell qty = 20 at price = 94.
The real_pnl does not change as the position has only increase (from a sell position, we just added more quantity at a different price). However, we have increase the unreal_pnl.

position_old_ = position_new_ = -10
position_new_ -= 20 = -30
open_vwap [0] += 0.0
open_vwap [1] += price * qty += 94 * 20 = 2850
vwap [0] = 0.0
vwap [1] = open_vwap [0] / abs(position_new_) = 95
real_pnl += 0 = -10
unreal_pnl = (price - vwap[1]) * position_new_ = 30

6. Execution: Sell qty = 10 at price = 90.

position_old_ = position_new_ = -30
position_new_ -= 10 = -40
open_vwap [0] += 0.0
open_vwap [1] += price * qty += 90 * 10 = 3750.0
vwap [0] = 0.0
vwap [1] = open_vwap [0] / abs(position_new_) = 93.75
real_pnl += 0 = -10
unreal_pnl = (price - vwap[1]) * position_new_ = 150


7. Execution: Buy qty = 40 at price = 88.
We don't swap indices as the position_new_ is still not positive, but zero. Instead, we no longer have open positions (not long nor short), so we set both open_swap[0] and open_swap[1] to zero (hence, also vwaps).  

position_old_ = position_new_ = -40
position_new_ += 40 = 0
open_vwap [0] = 0.0
open_vwap [1] = 0
vwap [0] = 0
vwap [1] = 0
real_pnl += (price - vwap[1]) * position_new_ = 220
unreal_pnl += 0


### the addFill() function
We got a client reponse from the trading strategy. Is this response a buy or a sell? If so, how much are we buying/selling and at what price.
* The side_index and opp_side_index will allow us to access and modify the opened positions at `open_vwap_`. The `side_value` will give us a plus/minus for a buying/selling side.  Therefore, we build the position:
* `position_` = `client_response->exec_qty` * `side_value`. It will be negative, if selling and positive if buying.
There are two things that can happen: (a) the response is to buy/sell, and we have things to buy/sell; or (b) the response is to buy/sell and we have things to sell/buy.
    (a) e.g., we had a buy position, now we got a buy request. We simply increase the open position, `open_vwap`, by (`client_response->price_` * `client_response->exec_qty_`);
    (b) e.g., we had a buy position, now we got a sell request. This will decrease the buy position:
        b.1. If it decreases the buy position without flipping, we update the buy position and we increase the realised pnl. Note the realised pnl is simply:
            * the difference between the response's price at which we are selling and the position's price at which they're buying. The difference is the gain/loss per trade. 
            * Then we multiply that rate by the amount we trade (that is, the minimum of either the full quantity that was up for buying, or the full quantity that we were looking to sell).
            * Finally, we multiply by either +/- to reflect the buying/selling side. 
                - If `(opp_side_vwap - client_response->price_) < 0`, and we were buying, that means we bought at a higher price than the price at which the goods were being sold, so we made a loss (the positive sign from `sideToValue(client_response->side_)` will keep it as a substraction); 
                - If `(opp_side_vwap - client_response->price_) < 0`, and we were selling, that means we sold at a higher price than the price at which the goods were being bought, so we made a gain (the negative sign from `sideToValue(client_response->side_)` will make it an addition); 
                - If `(opp_side_vwap - client_response->price_) > 0`, and we were buying, that means we bought at a lower price than the price at which the goods were being sold, so we made a gain (the positive sign from `sideToValue(client_response->side_)` will keep it as an addition); 
                - If `(opp_side_vwap - client_response->price_) > 0`, and we were selling, that means we sold at a lower price than the price at which the goods were being bought, so we made a loss (the negative sign from `sideToValue(client_response->side_)` will make it a substraction); 
        b.2. If it decreases the buy position causing it to flip, then we switch to the current position and update it; and we reset the old position. 
        b.3. If it decreases the buy position so that it leaves it at zero, then there are no open positions left. We set everything to zero.
The unrealised_pnl gets computed as the difference between the price that we are willing to buy/sell and the price at which they are willing to buy/sell, times the current position (positive if buy; negative if sell). Note that the realised_pnl is cumulative, while the unrealised_pnl is not.



# OrderManager
### which will be used by the strategies to send orders, manage them, and update these orders when there are updates

Contains 
- a `trade_engine` object, which stores the parent Trade Engine which uses this OrderManager;
- a `risk_manager` object, to perform pre-trade risk checks - that is, risk checks that are performed before new orders are sent out to the exchange.
- a `ticker_side_order_`, which is a hashmap, mapping ticker_id to OMOrderSideHashMap, which is a hash map of side to OMOrder (so three-dimensional array).

- `onOrderUpdate` takes a MEClientResponse (a reponse from the matching engine). We use the client's response ticker_id to obtain its matching OMOrder in the `ticker_side_order_`. According to the ClientResponseType, we update the OMOrder state.
- `newOrder` instatiates the input OMOrder pointer, assigning it the input ticker_id, price, side and qty; sets the OrderState to PENDING_NEW (as we will be now sending a request to the exchange, but we are pending of approval), and assigns it the `next_order_id_`. This `next_order_id_` is also used in the new request we generate and send to the exchange. It will allow us to match OMOrder to request.
- `cancelOrder` takes an existing OMOrder, modifies its OrderState to PENDING_CANCEL, and sends a CANCEL request to the exchange through the trade_engine_.
- `moveOrder` either sends a new order or cancels an existing order.
    * if `order_state_` is LIVE and the order's price does not match the input price, then we send a request to cancel this order. If the request gets accepted, the order_state_ will become DEAD and so we will go through the `order_state_` = DEAD case. 
    * if `order_state_` is DEAD, we check with the risk manager whether we can add this new order (with the new price), and if so, we send that request to the exchange.

# RiskManager
### to compute and check the market risk that a trading strategy is attempting to take on, as well as the risk it has realized
* Contains a `ticker_risk_` object, which is a hash map of ticker_id to RiskInfo object. A RiskInfo is a struct which holds both a PositionInfo and a RiskCfg objects. A RiskInfo object can use `checkPreTradeRisk` with a given side and quantity to check that:
- the qty given is less than the `max_order_size_`
- the current position_ + the additional position that we would gain with this order (sideToValue(side) * qty) is still less than `max_position_`;
- whether we can still afford to make another order given the current gain/loss: Is total_pnl still less than the `max_loss_`?
* Therefore the RiskManager constructor takes the two upper-level classes PositionKeeper (which holds the PositionInfo) and a TradeEngineCfgHashMap (which holds a ticker_id to RiskCfg hashmap), and initialises its own `ticker_risk_.position_info_` and `ticker_risk_.risk_cfg_` with these.
* Once initialised, the `checkPreTradeRisk` is able to call the `checkPreTradeRisk` for each RiskInfo in `ticker_risk_`.








