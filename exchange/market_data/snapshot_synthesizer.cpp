#include "snapshot_synthesizer.h"

namespace Exchange {
  SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                                           const std::string &snapshot_ip, int snapshot_port)
      : snapshot_md_updates_(market_updates), logger_("exchange_snapshot_synthesizer.log"), snapshot_socket_(logger_), order_pool_(ME_MAX_ORDER_IDS) {
    ASSERT(snapshot_socket_.init(snapshot_ip, iface, snapshot_port, /*is_listening*/ false) >= 0,
           "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    for(auto& orders : ticker_orders_)
      orders.fill(nullptr);
  }

  SnapshotSynthesizer::~SnapshotSynthesizer() {
    stop();
  }

  void SnapshotSynthesizer::start() {
    run_ = true;
    ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]() { run(); }) != nullptr,
           "Failed to start SnapshotSynthesizer thread.");
  }

  void SnapshotSynthesizer::stop() {
    run_ = false;
  }

  auto SnapshotSynthesizer::addToSnapshot(const MDPMarketUpdate *market_update) {
    const auto &me_market_update = market_update->me_market_update_;
    auto *orders = &ticker_orders_.at(me_market_update.ticker_id_);
    switch (me_market_update.type_) {
      case MarketUpdateType::ADD: {
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order == nullptr, "Received:" + me_market_update.toString() + " but order already exists:" + (order ? order->toString() : ""));
        orders->at(me_market_update.order_id_) = order_pool_.allocate(me_market_update);
      }
        break;
      case MarketUpdateType::MODIFY: {
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order does not exist.");
        ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
        ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

        order->qty_ = me_market_update.qty_;
        order->price_ = me_market_update.price_;
      }
        break;
      case MarketUpdateType::CANCEL: {
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order does not exist.");
        ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
        ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

        order_pool_.deallocate(order);
        orders->at(me_market_update.order_id_) = nullptr;
      }
        break;
      case MarketUpdateType::SNAPSHOT_START:
      case MarketUpdateType::CLEAR:
      case MarketUpdateType::SNAPSHOT_END:
      case MarketUpdateType::TRADE:
      case MarketUpdateType::INVALID:
        break;
    }

    ASSERT(market_update->seq_num_ == last_inc_seq_num_ + 1, "Expected incremental seq_nums to increase.");
    last_inc_seq_num_ = market_update->seq_num_;
  }

  auto SnapshotSynthesizer::publishSnapshot() {
    size_t snapshot_size = 0;

    const MDPMarketUpdate start_market_update{
      snapshot_size++, /*sequence number (=1, in this case)*/
      { MarketUpdateType::SNAPSHOT_START, 
        last_inc_seq_num_ /*order_id*/
      }
    };

    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), start_market_update.toString());
    snapshot_socket_.send(&start_market_update, sizeof(MDPMarketUpdate));

    // Loop over all ticker_ids
    for (size_t ticker_id = 0; ticker_id < ticker_orders_.size(); ++ticker_id) {
      // For each ticker_id...
      //  (1) obtain the array of order_ids
      const auto &orders = ticker_orders_.at(ticker_id);

      //  (2) Send a MDPMarketUpdate message of the MarketUpdateType::CLEAR type to 
      //      instruct the client to clear their order book before applying the messages 
      //      that follow
      MEMarketUpdate me_market_update;
      me_market_update.type_ = MarketUpdateType::CLEAR;
      me_market_update.ticker_id_ = ticker_id;

      const MDPMarketUpdate clear_market_update{snapshot_size++, me_market_update};
      logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), clear_market_update.toString());
      snapshot_socket_.send(&clear_market_update, sizeof(MDPMarketUpdate));

      // For each order that exists in the snapshot for this instrument, we publish a 
      //  MDPMarketUpdate message with MarketUpdateType::ADD till we have 
      //  published the information for all the orders
      for (const auto order: orders) {
        if (order) {
          const MDPMarketUpdate market_update{snapshot_size++, *order};
          logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), market_update.toString());
          snapshot_socket_.send(&market_update, sizeof(MDPMarketUpdate));
          snapshot_socket_.sendAndRecv();
        }
      }
    }

    // We publish a MDPMarketUpdate message of the MarketUpdateType::SNAPSHOT_END 
    //  type to mark the end of the snapshot messages. 
    // Note that we input last_inc_seq_num_ in both SNAPSHOT_START
    //  and SNAPSHOP_END as the OrderId value, so that the user can match the snapshot market data stream with the incremental market data stream
    const MDPMarketUpdate end_market_update{
      snapshot_size++, 
      {MarketUpdateType::SNAPSHOT_END, 
      last_inc_seq_num_ /*order_id*/
      }
    };
    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), end_market_update.toString());
    snapshot_socket_.send(&end_market_update, sizeof(MDPMarketUpdate));
    snapshot_socket_.sendAndRecv();

    logger_.log("%:% %() % Published snapshot of % orders.\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), snapshot_size - 1);
  }

  void SnapshotSynthesizer::run() {
    logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_));
    while (run_) {
      // Loop over elements in MDPMarketUpdateLFQueue (data that we pass from the matching engine into the snapshot synthesizer)
      for (auto market_update = snapshot_md_updates_->getNextToRead(); snapshot_md_updates_->size() && market_update; market_update = snapshot_md_updates_->getNextToRead()) {
        logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_),
                    market_update->toString().c_str());
        // Add each of these updates to ticker_orders_
        addToSnapshot(market_update);

        snapshot_md_updates_->updateReadIndex();
      }

      // If at least one minute has elapsed since last published snapshop, publish new snapshop
      if (getCurrentNanos() - last_snapshot_time_ > 60 * NANOS_TO_SECS) {
        last_snapshot_time_ = getCurrentNanos();
        publishSnapshot();
      }
    }
  }
}
