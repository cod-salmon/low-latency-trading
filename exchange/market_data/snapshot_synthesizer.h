#pragma once

#include "common/types.h"
#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/mcast_socket.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_data/market_update.h"
#include "matcher/me_order.h"

using namespace Common;

namespace Exchange {
  class SnapshotSynthesizer {
  public:
    SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                        const std::string &snapshot_ip, int snapshot_port);

    ~SnapshotSynthesizer();

    auto start() -> void;

    auto stop() -> void;

    auto addToSnapshot(const MDPMarketUpdate *market_update);

    auto publishSnapshot();

    auto run() -> void;

    // Deleted default, copy & move constructors and assignment-operators.
    SnapshotSynthesizer() = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &) = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &&) = delete;

    SnapshotSynthesizer &operator=(const SnapshotSynthesizer &) = delete;

    SnapshotSynthesizer &operator=(const SnapshotSynthesizer &&) = delete;

  private:
    MDPMarketUpdateLFQueue *snapshot_md_updates_ = nullptr;

    Logger logger_;

    volatile bool run_ = false;

    std::string time_str_;

    McastSocket snapshot_socket_;

    std::array<std::array<MEMarketUpdate *, ME_MAX_ORDER_IDS>, ME_MAX_TICKERS> ticker_orders_;

    // Variables to track information about the last incremental market data update. 
    size_t last_inc_seq_num_ = 0; // //to track the sequence number on the last incremental MDPMarketUpdate it has received. 
    Nanos last_snapshot_time_ = 0; // the last_snapshot_time_ variable used to track when the last snapshot was published over UDP since this component will only periodically publish the full snapshot of all the books.
    
    MemPool<MEMarketUpdate> order_pool_;
  };
}
