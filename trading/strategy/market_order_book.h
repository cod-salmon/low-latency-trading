#pragma once

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_order.h"
#include "exchange/market_data/market_update.h"

namespace Trading {
  class TradeEngine;

  class MarketOrderBook final {
  public:
    MarketOrderBook(TickerId ticker_id, Logger *logger);

    ~MarketOrderBook();

    auto onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept -> void;

    auto setTradeEngine(TradeEngine *trade_engine) {
      trade_engine_ = trade_engine;
    }

    auto updateBBO(bool update_bid, bool update_ask) noexcept {
      if(update_bid) {
        if(bids_by_price_) {
          bbo_.bid_price_ = bids_by_price_->price_;
          bbo_.bid_qty_ = bids_by_price_->first_mkt_order_->qty_;
          for(auto order = bids_by_price_->first_mkt_order_->next_order_; order != bids_by_price_->first_mkt_order_; order = order->next_order_)
            bbo_.bid_qty_ += order->qty_;
        }
        else {
          bbo_.bid_price_ = Price_INVALID;
          bbo_.bid_qty_ = Qty_INVALID;
        }
      }

      if(update_ask) {
        if(asks_by_price_) {
          bbo_.ask_price_ = asks_by_price_->price_;
          bbo_.ask_qty_ = asks_by_price_->first_mkt_order_->qty_;
          for(auto order = asks_by_price_->first_mkt_order_->next_order_; order != asks_by_price_->first_mkt_order_; order = order->next_order_)
            bbo_.ask_qty_ += order->qty_;
        }
        else {
          bbo_.ask_price_ = Price_INVALID;
          bbo_.ask_qty_ = Qty_INVALID;
        }
      }
    }

    auto getBBO() const noexcept -> const BBO* {
      return &bbo_;
    }

    auto toString(bool detailed, bool validity_check) const -> std::string;

    // Deleted default, copy & move constructors and assignment-operators.
    MarketOrderBook() = delete;

    MarketOrderBook(const MarketOrderBook &) = delete;

    MarketOrderBook(const MarketOrderBook &&) = delete;

    MarketOrderBook &operator=(const MarketOrderBook &) = delete;

    MarketOrderBook &operator=(const MarketOrderBook &&) = delete;

  private:
    const TickerId ticker_id_;

    TradeEngine *trade_engine_ = nullptr;

    OrderHashMap oid_to_order_;

    MemPool<MarketOrdersAtPrice> orders_at_price_pool_;
    MarketOrdersAtPrice *bids_by_price_ = nullptr;
    MarketOrdersAtPrice *asks_by_price_ = nullptr;

    OrdersAtPriceHashMap price_orders_at_price_;

    MemPool<MarketOrder> order_pool_;

    BBO bbo_;

    std::string time_str_;
    Logger *logger_ = nullptr;

  private:
    auto priceToIndex(Price price) const noexcept {
      return (price % ME_MAX_PRICE_LEVELS);
    }

    auto getOrdersAtPrice(Price price) const noexcept -> MarketOrdersAtPrice * {
      return price_orders_at_price_.at(priceToIndex(price));
    }

    auto addOrdersAtPrice(MarketOrdersAtPrice *new_orders_at_price) noexcept {
      // Add new orders to current orders at price hashmap
      price_orders_at_price_.at(priceToIndex(new_orders_at_price->price_)) = new_orders_at_price;
      // Obtain best bid/ask
      const auto best_orders_by_price = (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_);
      if (UNLIKELY(!best_orders_by_price)) { // if there are no best bids/asks, then new orders become our the bid/asks
        (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
        new_orders_at_price->prev_entry_ = new_orders_at_price->next_entry_ = new_orders_at_price;
      } else { // best bid/ask becomes our reference point.
        // We need to find out whether the new_orders should
        //  be placed before or after this target; and if after,
        //  by how much after. 
        auto target = best_orders_by_price;
        bool add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                          (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
        /* If !add_after now: CASE A - new_orders_at_price is willing to buy/sell at a higher/lower price than the current best bid/ask; else, add new_orders after target... Need to work out how much after */
        if (add_after) {
          target = target->next_entry_;
          add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                       (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
        }
        /* If !add_after now: CASE B - new_orders_at_price is the second best bid/ask; else, keep checking downwards. If still add_after, continue until hitting best_orders_by_price */
        while (add_after && target != best_orders_by_price) {
          add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                       (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
          if (add_after)
            target = target->next_entry_;
        }
        /* If !add_after now: CASE C - we found current target to be a less aggressive bid/ask than the one from new_orders_at_price; else, we hit best_orders_by_price: CASE D.*/
        if (add_after) {
          // In CASE D:
          if (target == best_orders_by_price) {
            target = best_orders_by_price->prev_entry_;
          }
          new_orders_at_price->prev_entry_ = target;
          target->next_entry_->prev_entry_ = new_orders_at_price;
          new_orders_at_price->next_entry_ = target->next_entry_;
          target->next_entry_ = new_orders_at_price;
        } else {
          // In CASE A, B, and C (we place new_orders_at_price before target):
          new_orders_at_price->prev_entry_ = target->prev_entry_;
          new_orders_at_price->next_entry_ = target;
          target->prev_entry_->next_entry_ = new_orders_at_price;
          target->prev_entry_ = new_orders_at_price;

          if ((new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ > best_orders_by_price->price_) ||
              (new_orders_at_price->side_ == Side::SELL &&
               new_orders_at_price->price_ < best_orders_by_price->price_)) {
            target->next_entry_ = (target->next_entry_ == best_orders_by_price ? new_orders_at_price
                                                                               : target->next_entry_);
            (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
          }
        }
      }
    }

    auto removeOrdersAtPrice(Side side, Price price) noexcept {
      const auto best_orders_by_price = (side == Side::BUY ? bids_by_price_ : asks_by_price_);
      auto orders_at_price = getOrdersAtPrice(price);

      if (UNLIKELY(orders_at_price->next_entry_ == orders_at_price)) { // empty side of book.
        (side == Side::BUY ? bids_by_price_ : asks_by_price_) = nullptr;
      } else {
        // Remove link to this MEOrderAtPrice from previous_ and and next_ entries
        orders_at_price->prev_entry_->next_entry_ = orders_at_price->next_entry_;
        orders_at_price->next_entry_->prev_entry_ = orders_at_price->prev_entry_;

        if (orders_at_price == best_orders_by_price) { 
          // Set best_orders to the order's next entry
          (side == Side::BUY ? bids_by_price_ : asks_by_price_) = orders_at_price->next_entry_;
        }
        // Set pointers to next_ and previous_ entries to null as this entry is being removed
        orders_at_price->prev_entry_ = orders_at_price->next_entry_ = nullptr;
      }

      // Empty entry at this price
      price_orders_at_price_.at(priceToIndex(price)) = nullptr;
      // Deallocate space for this entry in the memory pool
      orders_at_price_pool_.deallocate(orders_at_price);
    }

    auto removeOrder(MarketOrder *order) noexcept -> void {
      auto orders_at_price = getOrdersAtPrice(order->price_);

      if (order->prev_order_ == order) { // only one element.
        removeOrdersAtPrice(order->side_, order->price_);
      } else { // otherwise, just remove this entry and rearrange queue 
               // (also update its previous and next orders links)
        const auto order_before = order->prev_order_;
        const auto order_after = order->next_order_;
        order_before->next_order_ = order_after;
        order_after->prev_order_ = order_before;

        // Also: If order was first in the queue, put the one after as first
        if (orders_at_price->first_mkt_order_ == order) {
          orders_at_price->first_mkt_order_ = order_after;
        }
        // Once updated the link, reset this
        order->prev_order_ = order->next_order_ = nullptr;
      }

      oid_to_order_.at(order->order_id_) = nullptr;
      order_pool_.deallocate(order); // always empty memory queue
    }

    /*
      RECALL: There are two memory pools:
      (1) order_pool_ (where MEOrders are allocated to some bit of memory);
      (2) orders_at_price_pool_ (where MEOrdersAtPrice are allocated to some bit of memory)
      Difference between MEOrders and MEOrdersAtPrice is that 
        * MEOrder is just an object to store the matching engine order (with given ticket, client ids, etc.)
        * MEOrdersAtPrice keeps a queue of MEOrders for a given price. The queue is "saved" by keeping 
          the first MEOrder in the queue (first_me_order_) and the previous_ and next_ MEOrdersAtPrice entries 
          (for lower and higher prices respectively)
    */
    auto addOrder(MarketOrder *order) noexcept -> void {
      const auto orders_at_price = getOrdersAtPrice(order->price_);

      if (!orders_at_price) {
        //    if there is no order queue at that price, allocate the queue to the mem pool 
        //  and return the newly allocated object
        order->next_order_ = order->prev_order_ = order;

        auto new_orders_at_price = orders_at_price_pool_.allocate(order->side_, order->price_, order, nullptr, nullptr);
        addOrdersAtPrice(new_orders_at_price); // then add it to the price_orders_at_price_ hashmap.
      } else {
        auto first_order = (orders_at_price ? orders_at_price->first_mkt_order_ : nullptr);
        // 1. orders_at_price first_order
        // 2. order
        // 3. orders_at_price prev
        first_order->prev_order_->next_order_ = order;
        order->prev_order_ = first_order->prev_order_;
        order->next_order_ = first_order;
        first_order->prev_order_ = order;
      }

      oid_to_order_.at(order->order_id_) = order;
    }
  };

  typedef std::array<MarketOrderBook *, ME_MAX_TICKERS> MarketOrderBookHashMap;
}
