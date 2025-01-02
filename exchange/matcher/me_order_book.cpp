#include "me_order_book.h"

#include "matcher/matching_engine.h"

namespace Exchange {
  MEOrderBook::MEOrderBook(TickerId ticker_id, Logger *logger, MatchingEngine *matching_engine)
      : ticker_id_(ticker_id), matching_engine_(matching_engine), orders_at_price_pool_(ME_MAX_PRICE_LEVELS), order_pool_(ME_MAX_ORDER_IDS),
        logger_(logger) {
  }

  MEOrderBook::~MEOrderBook() {
    logger_->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                toString(false, true));

    matching_engine_ = nullptr;
    bids_by_price_ = asks_by_price_ = nullptr;
    for (auto &itr: cid_oid_to_order_) {
      itr.fill(nullptr);
    }
  }

  /*
  Match order specified by {ticker_id, client_id, side, client_order_id, new_market_order_id}
  against "itr", and modify "leaves_qty" accordingly (how much is left after the matching)
  */
  auto MEOrderBook::match(TickerId ticker_id, ClientId client_id, Side side, OrderId client_order_id, OrderId new_market_order_id, MEOrder* itr, Qty* leaves_qty) noexcept {
    const auto order = itr;
    const auto order_qty = order->qty_;
    const auto fill_qty = std::min(*leaves_qty, order_qty); 

    *leaves_qty -= fill_qty; // managed to buy fill_qty amount; we have leaves_qty left. 
    order->qty_ -= fill_qty; // got this amount from the passive orders

    // Client response: We filled the request: 
    //  - we got fill_qty amount (we "executed" fill_qty)
    //  - from passive order at price itr->price_
    //  - and have leaves_qty left from that matching transaction

    // Response to the client who sent the new_order and got matched
    client_response_ = {ClientResponseType::FILLED, client_id, ticker_id, client_order_id,
                        new_market_order_id, side, itr->price_, fill_qty, *leaves_qty};
    matching_engine_->sendClientResponse(&client_response_);

    // Response to the client who had an order in the matching engine pending to be matched
    client_response_ = {ClientResponseType::FILLED, order->client_id_, ticker_id, order->client_order_id_,
                        order->market_order_id_, order->side_, itr->price_, fill_qty, order->qty_};
    matching_engine_->sendClientResponse(&client_response_);

    // Update the public: There was a trading transaction with 
    //  itr->price_ and fill_qty execution price and quantity
    market_update_ = {MarketUpdateType::TRADE, OrderId_INVALID, ticker_id, side, itr->price_, fill_qty, Priority_INVALID};
    matching_engine_->sendMarketUpdate(&market_update_);

    if (!order->qty_) { // If there's no more left in the passive order, remove it (cancel it)
      // update the public that this order has been cancelled
      market_update_ = {MarketUpdateType::CANCEL, order->market_order_id_, ticker_id, order->side_,
                        order->price_, order_qty, Priority_INVALID};
      matching_engine_->sendMarketUpdate(&market_update_);
      // and remove it from the order book
      removeOrder(order);
    } else {
      // simply update the public with an order modification (its new reduced quantity in the book)
      market_update_ = {MarketUpdateType::MODIFY, order->market_order_id_, ticker_id, order->side_,
                        order->price_, order->qty_, order->priority_};
      matching_engine_->sendMarketUpdate(&market_update_);
    }
  }

  auto MEOrderBook::checkForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty, Qty new_market_order_id) noexcept {
    auto leaves_qty = qty;

    if (side == Side::BUY) {
      while (leaves_qty && asks_by_price_) { // While we still have quantity behind and we haven't hit the
                                             //   end of the asks_by_price queue
        const auto ask_itr = asks_by_price_->first_me_order_;
        if (LIKELY(price < ask_itr->price_)) {
          // Once price at which we are willing to buy is less than 
          //  the ask price in the asks_by_price, then we stop (nothing to buy anymore!)
          break;
        }
        // Until then, keep matching and checking how much quantity we have left (leaves_qty).
        // Match order specified by {icker_id, client_id, side, client_order_id, new_market_order_id}
        //  against "ask_itr", and modify "leaves_qty" accordingly (how much is left after the matching)
        match(ticker_id, client_id, side, client_order_id, new_market_order_id, ask_itr, &leaves_qty);
      }
    }
    if (side == Side::SELL) {
      while (leaves_qty && bids_by_price_) {
        const auto bid_itr = bids_by_price_->first_me_order_;
        if (LIKELY(price > bid_itr->price_)) {
          break;
        }
        // Match order specified by {icker_id, client_id, side, client_order_id, new_market_order_id}
        //  against "bid_itr", and modify "leaves_qty" accordingly (how much is left after the matching)
        match(ticker_id, client_id, side, client_order_id, new_market_order_id, bid_itr, &leaves_qty);
      }
    }

    return leaves_qty; // Returns leaves_qty, if there's a match leaves_qty < qty.
  }

  auto MEOrderBook::add(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty) noexcept -> void {
    // Accept new order and check for matches in the order book
    const auto new_market_order_id = generateNewMarketOrderId();
    client_response_ = {ClientResponseType::ACCEPTED, client_id, ticker_id, client_order_id, new_market_order_id, side, price, 0, qty};
    matching_engine_->sendClientResponse(&client_response_);

    const auto leaves_qty = checkForMatch(client_id, client_order_id, ticker_id, side, price, qty, new_market_order_id);

    if (LIKELY(leaves_qty)) { 
      // There was a match. We sent a MODIFY response to the market (invalidated priority number)
      // We need to generate a new order with the new quantity: 
      //  get next priority and allocate new order to a bit of memory in the pool
      // ** NOTE: MemPool::allocate() adds MEOrder to memory pool (this memory pool does not consider priority 
      //    or anything w.r.t. the MEOrder object; it just assigns the object to some memory queue) and 
      //    returns a pointer to the MEOrder object in that bit of memory.
      const auto priority = getNextPriority(price);
      auto order = order_pool_.allocate(ticker_id, client_id, client_order_id, new_market_order_id, 
                                        side, price, leaves_qty, priority, nullptr, nullptr);
      
      addOrder(order); // add new order to orders_at_price
      // Update market with newly added order
      market_update_ = {MarketUpdateType::ADD, new_market_order_id, ticker_id, side, price, leaves_qty, priority};
      matching_engine_->sendMarketUpdate(&market_update_);
    }
  }

  auto MEOrderBook::cancel(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void {
    auto is_cancelable = (client_id < cid_oid_to_order_.size()); // there's one array element per allowed client_id
    MEOrder *exchange_order = nullptr;
    if (LIKELY(is_cancelable)) { // there is such entry in the hashmap - might be empty tho, so check
      auto &co_itr = cid_oid_to_order_.at(client_id); // get the array of MEOrders, ordered by client_order_id
      exchange_order = co_itr.at(order_id); // get the MEOrder for that client_order_id
      is_cancelable = (exchange_order != nullptr); // is cancellable if this array of MEOrders is not empty
    }

    if (UNLIKELY(!is_cancelable)) { // Just update the client that we cannot cancel such order
                                    //  (no need to share info with the market)
      client_response_ = {ClientResponseType::CANCEL_REJECTED, client_id, ticker_id, order_id, OrderId_INVALID,
                          Side::INVALID, Price_INVALID, Qty_INVALID, Qty_INVALID};
    } else {
      // Invalidate quantity
      client_response_ = {ClientResponseType::CANCELED, client_id, ticker_id, order_id, exchange_order->market_order_id_,
                          exchange_order->side_, exchange_order->price_, Qty_INVALID, exchange_order->qty_};
      // Update market that the book order has been cancelled (removed)
      market_update_ = {MarketUpdateType::CANCEL, exchange_order->market_order_id_, ticker_id, exchange_order->side_, exchange_order->price_, 0,
                        exchange_order->priority_};

      removeOrder(exchange_order); // remove it from all hashmaps: orders_at_price and cid_oid_to_order, 
                                   // and all links to it. Also, deallocate memory and so on.

      matching_engine_->sendMarketUpdate(&market_update_);
    }

    matching_engine_->sendClientResponse(&client_response_);
  }

  auto MEOrderBook::toString(bool detailed, bool validity_check) const -> std::string {
    std::stringstream ss;
    std::string time_str;

    auto printer = [&](std::stringstream &ss, MEOrdersAtPrice *itr, Side side, Price &last_price, bool sanity_check) {
      char buf[4096];
      Qty qty = 0;
      size_t num_orders = 0;

      // All orders with the same price are assigned to the same array, sorted by FIFO, first one being first_me_order
      // Look over orders with the same price (itr->price_), to get the net number of orders and the net quantity for this order_id and this price
      for (auto o_itr = itr->first_me_order_;; o_itr = o_itr->next_order_) { // For every queue (array) of MEOrders, 
        // iterator starts with first_me_order, which moves around the hashmap by using the next_order_ pointer.
        qty += o_itr->qty_; // increase order quantity at that price 
        ++num_orders; // increase number of orders with this order_id
        if (o_itr->next_order_ == itr->first_me_order_) // end of loop
          break;
      }
      // Assign string to buf, then add buf to sstream
      sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)", 
              /* px: this price we are looking at --  p: the previous price -- n: the next price*/
              priceToString(itr->price_).c_str(), priceToString(itr->prev_entry_->price_).c_str(), priceToString(itr->next_entry_->price_).c_str(),
              /* We are making "num_orders" of "qty" size at price "itr->price_" */
              priceToString(itr->price_).c_str(), qtyToString(qty).c_str(), std::to_string(num_orders).c_str());
      ss << buf;

      for (auto o_itr = itr->first_me_order_;; o_itr = o_itr->next_order_) {
        if (detailed) { // show the order_id for the current order, the previous and the next
          sprintf(buf, "[oid:%s q:%s p:%s n:%s] ",
                  orderIdToString(o_itr->market_order_id_).c_str(), qtyToString(o_itr->qty_).c_str(),
                  orderIdToString(o_itr->prev_order_ ? o_itr->prev_order_->market_order_id_ : OrderId_INVALID).c_str(),
                  orderIdToString(o_itr->next_order_ ? o_itr->next_order_->market_order_id_ : OrderId_INVALID).c_str()
                  );
          ss << buf;
        }

        if (o_itr->next_order_ == itr->first_me_order_) // end of loop
          break;
      }

      ss << std::endl;

      if (sanity_check) {
        if ((side == Side::SELL && last_price >= itr->price_) || (side == Side::BUY && last_price <= itr->price_)) {
          FATAL("Bids/Asks not sorted by ascending/descending prices last:" + priceToString(last_price) + " itr:" + itr->toString());
        }
        last_price = itr->price_;
      }
    };

    ss << "Ticker:" << tickerIdToString(ticker_id_) << std::endl;
    {
      auto ask_itr = asks_by_price_;
      auto last_ask_price = std::numeric_limits<Price>::min();
      // Loop over all asks, ordered by price.
      for (size_t count = 0; ask_itr; ++count) { 
        ss << "ASKS L:" << count << " => ";
        auto next_ask_itr = (ask_itr->next_entry_ == asks_by_price_ ? nullptr : ask_itr->next_entry_);
        printer(ss, ask_itr, Side::SELL, last_ask_price, validity_check);
        ask_itr = next_ask_itr;
      }
    }

    ss << std::endl << "                          X" << std::endl << std::endl;

    {
      auto bid_itr = bids_by_price_;
      auto last_bid_price = std::numeric_limits<Price>::max();
      for (size_t count = 0; bid_itr; ++count) {
        ss << "BIDS L:" << count << " => ";
        auto next_bid_itr = (bid_itr->next_entry_ == bids_by_price_ ? nullptr : bid_itr->next_entry_);
        printer(ss, bid_itr, Side::BUY, last_bid_price, validity_check);
        bid_itr = next_bid_itr;
      }
    }

    return ss.str();
  }
}
