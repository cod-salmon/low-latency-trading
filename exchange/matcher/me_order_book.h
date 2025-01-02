#pragma once

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"
#include "order_server/client_response.h"
#include "market_data/market_update.h"

#include "me_order.h"

using namespace Common;

namespace Exchange {
  class MatchingEngine;

  class MEOrderBook final {
  public:
    explicit MEOrderBook(TickerId ticker_id, Logger *logger, MatchingEngine *matching_engine);

    ~MEOrderBook();

    auto add(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty) noexcept -> void;

    auto cancel(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void;

    auto toString(bool detailed, bool validity_check) const -> std::string;

    // Deleted default, copy & move constructors and assignment-operators.
    MEOrderBook() = delete;

    MEOrderBook(const MEOrderBook &) = delete;

    MEOrderBook(const MEOrderBook &&) = delete;

    MEOrderBook &operator=(const MEOrderBook &) = delete;

    MEOrderBook &operator=(const MEOrderBook &&) = delete;

  private:
    TickerId ticker_id_ = TickerId_INVALID;

    MatchingEngine *matching_engine_ = nullptr;

    ClientOrderHashMap cid_oid_to_order_;

    MemPool<MEOrdersAtPrice> orders_at_price_pool_;
    MemPool<MEOrder> order_pool_;

    MEOrdersAtPrice *bids_by_price_ = nullptr;
    MEOrdersAtPrice *asks_by_price_ = nullptr;

    OrdersAtPriceHashMap price_orders_at_price_;

    MEClientResponse client_response_;
    MEMarketUpdate market_update_;

    OrderId next_market_order_id_ = 1;

    std::string time_str_;
    Logger *logger_ = nullptr;

  private:
    auto generateNewMarketOrderId() noexcept -> OrderId {
      return next_market_order_id_++;
    }

    auto priceToIndex(Price price) const noexcept {
      return (price % ME_MAX_PRICE_LEVELS);
    }

    auto getOrdersAtPrice(Price price) const noexcept -> MEOrdersAtPrice * {
      return price_orders_at_price_.at(priceToIndex(price));
    }

    auto addOrdersAtPrice(MEOrdersAtPrice *new_orders_at_price) noexcept {
      // Add new orders to current orders at price hashmap
      price_orders_at_price_.at(priceToIndex(new_orders_at_price->price_)) = new_orders_at_price;
      // Also add new orders to either bids/asks depending on its side
      const auto best_orders_by_price = (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_);
      if (UNLIKELY(!best_orders_by_price)) { // if there are no bids/asks, put new orders as the bid/asks
        (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
        new_orders_at_price->prev_entry_ = new_orders_at_price->next_entry_ = new_orders_at_price;
      } else {
        auto target = best_orders_by_price;
        // add_after: to track whether to add new orders before/after the current target
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
              (new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ < best_orders_by_price->price_)) {
            // If: CASE B; else: not CASE A:
            target->next_entry_ = (target->next_entry_ == best_orders_by_price ? 
                                   new_orders_at_price :    /* CASE B */
                                   target->next_entry_);    /* any not CASE A */
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

        if (orders_at_price == best_orders_by_price) { // Readjust best_orders to the order's next entry
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

    auto getNextPriority(Price price) noexcept {
      const auto orders_at_price = getOrdersAtPrice(price);
      if (!orders_at_price)
        return 1lu;

      return orders_at_price->first_me_order_->prev_order_->priority_ + 1;
    }

    auto match(TickerId ticker_id, ClientId client_id, Side side, OrderId client_order_id, OrderId new_market_order_id, MEOrder* bid_itr, Qty* leaves_qty) noexcept;

    auto
    checkForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty, Qty new_market_order_id) noexcept;

    auto removeOrder(MEOrder *order) noexcept {
      auto orders_at_price = getOrdersAtPrice(order->price_); // what if entry for that order does not exist?? Need some assert

      if (order->prev_order_ == order) { // only one element at this price in queue
        removeOrdersAtPrice(order->side_, order->price_);
      } else { // otherwise, just remove this entry and rearrange queue 
               // (also update its previous and next orders links)
        const auto order_before = order->prev_order_;
        const auto order_after = order->next_order_;
        order_before->next_order_ = order_after;
        order_after->prev_order_ = order_before;

        // Also: If order was first in the queue, put the one after as first
        if (orders_at_price->first_me_order_ == order) {
          orders_at_price->first_me_order_ = order_after;
        }
        // Once updated the link, reset this
        order->prev_order_ = order->next_order_ = nullptr;
      }

      cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = nullptr;
      order_pool_.deallocate(order); // always empty memory queue
    }

    /*
      There are two memory pools:
      (1) order_pool_ (where MEOrders are allocated to some bit of memory);
      (2) orders_at_price_pool_ (where MEOrdersAtPrice are allocated to some bit of memory)
      Difference between MEOrders and MEOrdersAtPrice is that 
        * MEOrder is just an object to store the matching engine order (with given ticket, client ids, etc.)
        * MEOrdersAtPrice keeps a queue of MEOrders for a given price. The queue is "saved" by keeping 
          the first MEOrder in the queue (first_me_order_) and the previous_ and next_ MEOrdersAtPrice entries 
          (for lower and higher prices respectively)
    */
    auto addOrder(MEOrder *order) noexcept {
      // Get order queue at that price; 
      const auto orders_at_price = getOrdersAtPrice(order->price_);

      if (!orders_at_price) {
        //    if there is no order queue at that price, allocate the queue to the mem pool and return the object
        //    newly allocated in memory
        order->next_order_ = order->prev_order_ = order;

        auto new_orders_at_price = orders_at_price_pool_.allocate(order->side_, order->price_, order, nullptr, nullptr);
        addOrdersAtPrice(new_orders_at_price); // then add it to the price_orders_at_price_ hashmap.
      } else {
        auto first_order = (orders_at_price ? orders_at_price->first_me_order_ : nullptr);
        // 1. orders_at_price first_order
        // 2. order
        // 3. orders_at_price prev
        first_order->prev_order_->next_order_ = order;
        order->prev_order_ = first_order->prev_order_;
        order->next_order_ = first_order;
        first_order->prev_order_ = order;
      }

      // What if entry already existed... (we just substitute)?
      cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = order;
    }
  };

  typedef std::array<MEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}
