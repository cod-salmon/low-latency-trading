# The PositionKeeper class
Keeps track of the total profits and losses (PnL) for each `ticker_id_` (i.e., stock, market, etc.). Therefore, it holds an array of `PositionInfo` structs ordered by `ticker_id_`.

### A `PositionInfo` struct
This struct holds:
- `total_pnl_`, for the sum of the unrealised and the realised pnl.
- `real_pnl_`, for the pnl from positions that have been closed;
- `unreal_pnl_`, for the pnl from currently open positions;

## PositionKeeper::addFill
It receives a MEClientResponse from the trading strategy. 
- we set `old_position` to be the previous position, kept at `position_`.
- `position_` keeps a sum of products of `exec_qty_` by the `side_value`. We update it by adding to it the current MEClientResponse's `exec_qty_ * side_value`. 
- The `side_value` equals -1 for a sell, and +1 for a buy. 
- `volume_`, on the other hand, keeps only a sum of `exec_qty_`.
- Note `old_position` keeps the previous sum of products of `exec_qty_`s by `side_value`s. If it is negative, that means we previously were on the selling side; if it is positive, that means we previously were on the buying side. `side_value` on the other hand, gets updated from the current MEClientResponse. 
    - If the product `old_position * side_value` is greater or equal to zero, then the current MEClientResponse holds the same side as the previous positions. We therefore just need to update the open volume-weighted average price or `open_vwap_` for the respective buy/sell side. `open_vwap_` keeps track of the total amount that one would gain if she/he sold each `exec_qty_` at their given `price_` from the trading engine. We thus add to the `open_vwap_` at the specific `side_index`, the product of the current MEClientResponse's `exec_qty_` by the MEClientResponse's `price_`.
    - If the product `old_position * side_value` is negative, then the current MEClientResponse is asking for a buy/sell when the previous positions were for sell/buy, and we should have some realised PnL (that is, we are able to close or partially close the buy/sell request, and have some realised profits or losses). How much profits/losses were made gets calculated as follows:  
    1. We work out how much we are able to buy/sell given the available offers. Thus, we take the minimum between the MEClientResponse's `exec_qty_` and the previous position (which keeps track of the total amount on offer); and multiply it by
    2. the difference between what the volume-weighted average price of what is on offer is (`opp_side_vwap`), and the current MEClientResponse's `price_`; and multiply it by
    3. +/- 1, depending on the side of the MEClientResponse.
    Note `opp_side_vwap` is obtained by dividing the `open_vwap_` at the opposite `side_index` to the one obtained from the MEClientResponse, and dividing it by the previous volume (`old_position`). Also note that `open_vwap_`  at the opposite `side_index` to the one obtained from the MEClientResponse gets updated right after to the product `opp_side_vwap` times the current `position_` (effectively, the volume-weighted average price of what we have on offer changes as the volume changes).
- If the product `position_ * old_position` is less than zero, then we have flipped position. What currently is on offer is sell/buy rather than buy/sell from previously. We update the `open_vwap_` at `side_index` to be the current MEClientResponse's `price_` times the absolute value of the current `position`; and we set the opposite side's `open_vwap_` to zero.
- If `position_` is 0, then sells/buys have cancelled each other out, so `open_vwap_` for both `side_index`s becomes zero, as well as the `unreal_pnl_`. If`position_` is positive, then we should have some `unreal_pnl` equal to the current MEClientResponse's `price_` minus the `open_vwap_` price at the specific `side_index` divided by the absolute value of the current `position_`, all together divided by the absolute value of the current `position_`. If `position_` is negative, then we should have some `unreal_pnl_` equal to the `open_vwap_` price at the specific `side_index` divided by the absolute value of the current `position_` minus the current MEClientResponse's `price_`, altogether divided by the absolute value of the current `position_`.
- At the end of `addFill`, we compute the `total_pnl_` as the sum of the `unreal_pnl` and the `real_pnl`.

### Example 1
Let's assume we receive the MEClientResponse to buy 10 units of some stock at `price=100`.
- `old_position` is zero as we have no previous positions; `side_index` is 0 (as we are on the BUY side), and `opp_side_index` is therefore 1. The `side_value` is +1 as we are on the buying side. 
- `position_ = position_ + exec_qty * side_value = 0 + 10 * 1 = 10`.
- As `old_position * side_value = 0`, then we move to CASE A: we compute `open_vwap[0] = open_vwap[0] + price * exec_qty = 0 + 100 * 10 = 1000`. Parallely, `open_vwap[1]` (the sell side), remains unchanged and so equal to zero.
- As `position > 0`, we enter CASE E. We compute `unreal_pnl = (price - open_vwap[0]/abs(position_)) * abs(position_) = (100 - 1000/10) * 10 = 0`. Parallely, `real_pnl` remains unchanged and so equal to zero.
- `total_pnl_ = unreal_pnl_ + real_pnl_ = 0 + 0 = 0`

### Example 2
Let's assume now we receive another MEClientResponse to buy 10 units of the same stock at `price=90`.
- `old_position = position_ = 10`;`side_index` is 0 (as we are on the BUY side), and `opp_side_index` is therefore 1. The `side_value` is +1 as we are on the buying side. 
- `position_ = position_ + exec_qty * side_value = 10 + 10 * 1 = 20`.
- As `old_position * side_value = 10 > 0`, then we move to CASE A: we compute `open_vwap_[0] = open_vwap_[0] + price * exec_qty = 1000 + 90 * 10 = 1900`. Parallely, `open_vwap_[1]` (the sell side), remains unchanged and so equal to zero.
- As `position > 0`, we enter CASE E. We compute `unreal_pnl = (price - open_vwap_[0]/abs(position_)) * abs(position_) = (90 - 1900/20) * 20 = -100`. Parallely, `real_pnl` remains unchanged and so equal to zero.
- `total_pnl_ = unreal_pnl_ + real_pnl_ = -100 + 0 = -100`.

### Example 3
Let's assume now we receive another MEClientResponse to sell 10 units of the same stock at `price=92`.
- `old_position = position_ = 20`;`side_index` is now 1 (as we are on the SELL side), and `opp_side_index` is therefore 0. The `side_value` is -1 as we are on the selling side. 
- `position_ = position_ + exec_qty * side_value = 20 + 10 * (-1) = 10`.
- As `old_position * side_value < 0`, we move to CASE B. Because we are going to close some or the whole request, the buys volume will change and so the buy's volume-weighted average price. We therefore recompute it `opp_side_vwap = open_vwap_[opp_side_index] / std::abs(old_position) = 1900 / 20 = 95`. Thus, `open_vwap_[opp_side_index] = opp_side_vwap * std::abs(position_) = 95 * 10 = 950` (the buy side). `open_vwap_[side_index] = open_vwap_[1]` (the sell side) remains unchanged and so equal to zero, as all sell offers remain after execution.
- The `real_pnl` after execution gets computed as: `real_pnl = real_pnl + min(exec_qty, abs(old_position) * (opp_side_vwap - price) * (-1)) = 0 + min(10, 20) * (95 - 92) * (-1) = -30`.
- As `position > 0`, we enter CASE E. We compute `unreal_pnl = (price - open_vwap[0]/abs(position_)) * abs(position_) = (92 - 950/10) * 10 = -30`. 
- `total_pnl_ = unreal_pnl_ + real_pnl_ = -30 - 30 = -60`

### Example 4
Let's assume that we get another MEClientResponse for selling 20 units at `price=97`.
- `old_position = position_ = 10`;`side_index` is still 1 (as we are on the SELL side), and `opp_side_index` is therefore 0. The `side_value` is -1 as we are on the selling side. 
- `position_ = position_ + exec_qty * side_value = 10 + 20 * (-1) = -10`.
- As `old_position * side_value < 0`, we move to CASE B. Because we are going to close some or the whole request, the buys volume will change and so the buy's volume-weighted average price. We therefore recompute it `opp_side_vwap = open_vwap_[opp_side_index] / std::abs(old_position) = 950 / 10 = 95`. Thus, `open_vwap_[opp_side_index] = opp_side_vwap * std::abs(position_) = 95 * 10 = 950` (the buy side). The `real_pnl` after execution gets computed as: `real_pnl = real_pnl + min(exec_qty, abs(old_position) * (opp_side_vwap - price) * (-1)) = -30 + min(20, 10) * (95 - 97) * (-1) = -10`.
- As `position * old_position < 0`, we move to CASE C. `open_vwap[1] = price * abs(position_) = 97 * 10 = 970` and we clear `open_vwap_[opp_side_index] = 0`, as we have switched to the sell side now.
- As `position < 0`, we enter CASE F. We compute `unreal_pnl = (price - open_vwap[1]/abs(position_)) * abs(position_) = (97 - 970/10) * 10 = 0`. 
- `total_pnl_ = unreal_pnl_ + real_pnl_ = 0 - 10 = -10`.

### Example 5
Let's assume we get another MEClientResponse for selling 20 units at `price=94`. 
- `old_position = position_ = -10`; `side_index` is still 1 (as we are on the SELL side), and `opp_side_index` is therefore 0. The `side_value` is -1 as we are on the selling side. 
- `position_ = position_ + exec_qty * side_value = -10 + 20 * (-1) = -30`.
- As `old_position * side_value > 0`, we pass to CASE A and compute `open_vwap_[1] = open_vwap_[1] + price * exec_qty = 970 + 94 * 20 = 2850`. Parallely, `open_vwap_[0]` (the buy side), remains zero, as it was reset on Example 4, and we remain on the sell side. `real_pnl` remains unchanged and so being equal to -10.
- As `position < 0`, we move to CASE F: `unreal_pnl = (open_vwap[1]/abs(position) - price) * abs(position) = (2850/30 - 94) * 30 = 30`
- `total_pnl_ = unreal_pnl_ + real_pnl_ = 30 - 10 = 20`

### Example 6
Let's assume we get another MEClientResponse for selling 10 units at `price=90`. 
- `old_position = position_ = -30`; `side_index` is still 1 (as we are on the SELL side), and `opp_side_index` is therefore 0. The `side_value` is -1 as we are on the selling side. 
- `position_ = position_ + exec_qty * side_value = -30 + 10 * (-1) = -40`.
- As `old_position * side_value > 0`, we pass to CASE A and compute `open_vwap_[1] = open_vwap_[1] + price * exec_qty = 2850 + 90 * 10 = 3750`. Parallely, `open_vwap_[0]` (the buy side), remains zero, as it was reset on Example 4, and we still remain on the sell side. `real_pnl` remains unchanged and so being equal to -10.
- As `position_ < 0`, we move to CASE F: `unreal_pnl = (open_vwap[1]/abs(position) - price) * abs(position) = (3750/40 - 90) * 40 = 150`.
- `total_pnl_ = unreal_pnl_ + real_pnl_ = 150 - 10 = 140`

### Example 7
Let's assume we receive a final MEClientResponse for buying 40 units at `price=88`. 
- `old_position = position_ = -40`; `side_index` is now 0 (as we are on the SELL side), and `opp_side_index` is therefore 1. The `side_value` is 1 as we are back on the buying side. 
- `position_ = position_ + exec_qty * side_value = -40 + 40  = 0`.
- As `old_position * side_value < 0`, we pass to CASE B. Because we are going to close some or the whole request, the sells volume will change and so the sell's volume-weighted average price. We therefore recompute it `opp_side_vwap = open_vwap_[opp_side_index] / std::abs(old_position) = 3750 / 40 = 93.75`. Thus, `open_vwap_[opp_side_index] = opp_side_vwap * std::abs(position_) = 93.75 * 0 = 0` (the sell side). The `real_pnl` after execution gets computed as: `real_pnl = real_pnl + min(exec_qty, abs(old_position) * (opp_side_vwap - price) * (-1)) = -10 + min(40, 40) * (93-75 - 88) * 1 = -10 + 230 = 220`.
- As `position_ = 0`, we move to CASE D, set both `open_vwap[0] = open_vwap[1] = 0` and the `unreal_pnl_ = 0`.

## PositionKeeper::updateBBO
This function gets called when an update on the market order book takes place. An update on the market order book triggers an update on its own BBO, but also on the trading engine and all of its constituents, including the position keeper.
`PositionKeeper::updateBBO` will update its internal `bbo_` to the input `bbo` and readjust the `unreal_pnl_`. As the market order book changes, so may change the market price, represented by `mid_price` and being equal to the arithmetic mean of the best bid price and the best ask price. As the market price changes, the potential profits/losses that can be made buy buying/selling (that is, effectively the unrealised PnL) also change. Therefore if the current position is on the buy side, we readjust the `unreal_pnl` to be the difference between the `mid_price` and the `open_vwap[0]/abs(position_)` multiplied by `abs(position_)`; if they current position is on the sell side, we readjust the `unreal_pnl` to be the diffence between the `open_vwap[1]/abs(position_)` and `mid_price`, multiplied by `abs(position_)`. Once `unrealised_pnl` gets readjusted, we need to also readjust `total_pnl`.  