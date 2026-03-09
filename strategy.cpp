#include "strategy.h"

#include <cmath>
#include <string>

#include "tracer.h"
#include "util.h"

Strategy::Strategy(std::shared_ptr<Bitget> client) : client_(client) {
  Init();
}

Strategy::~Strategy() { stop(); }

void Strategy::start() {
  thread_ = std::make_shared<Poco::Thread>();
  thread_->setName(instId_.substr(0, 3));
  if (!thread_running_ && thread_) {
    thread_->start(*this);
    thread_running_ = true;
  }
}

void Strategy::stop() {
  thread_running_ = false;
  if (thread_) {
    thread_->join();
    thread_ = nullptr;
  }
}

void Strategy::wait() {
  if (thread_ && thread_->isRunning()) {
    thread_->join();
  }
}

void Strategy::Init() {
  instId_ = client_->getInstId();
  client_->setLeverage(kConfig.lever);
  NOTICE("Strategy initialized for " << instId_);
}

bool Strategy::UpdateFundingRate() {
  return client_->fundingRate(funding_rate_);
}

// Open a market position in the direction that earns the funding fee.
// If rate > 0 (longs pay shorts), go SHORT to receive the fee.
// If rate < 0 (shorts pay longs), go LONG to receive the fee.
bool Strategy::OpenPosition() {
  Ticker tk;
  if (!client_->tickers(tk)) {
    ERROR("OpenPosition: failed to get ticker for " << instId_);
    return false;
  }

  // Calculate contract size from USDT notional
  float size = kConfig.input * kConfig.lever / tk.lastPr;
  size = std::stof(
      adjustDecimalPlaces(size, client_->getSymbol().volumePlace));
  if (size <= 0) {
    ERROR("OpenPosition: calculated size is zero for " << instId_);
    return false;
  }

  Order order;
  order.symbol = instId_;
  order.size = size;
  order.side = (funding_rate_.rate > 0) ? "sell" : "buy";

  if (!client_->placeMarketOrder(order)) {
    ERROR("OpenPosition: failed to place market order for " << instId_);
    return false;
  }

  settlement_time_ = funding_rate_.nextFundingTime;
  NOTICE("Opened " << order.side << " position for " << instId_
                   << ", size=" << size
                   << ", fundingRate=" << funding_rate_.rate
                   << ", settlement=" << settlement_time_);
  return true;
}

bool Strategy::ClosePosition() {
  if (!client_->closePosition()) {
    ERROR("ClosePosition: failed to close position for " << instId_);
    return false;
  }
  NOTICE("Closed position for " << instId_);
  return true;
}

bool Strategy::HasPosition() {
  if (!client_->singlePosition(position_)) return false;
  return position_.total > 0;
}

void Strategy::run() {
  DEBUG("Strategy start running " << instId_);
  while (thread_running_) {
    if (!UpdateFundingRate()) {
      SLEEP_MS(STRATEGY_POLL_INTERVAL_MS);
      continue;
    }

    bool in_position = HasPosition();
    uint64_t now_ms = getCurrentTimeMs();
    int64_t time_to_settlement =
        static_cast<int64_t>(funding_rate_.nextFundingTime) -
        static_cast<int64_t>(now_ms);

    if (!in_position && !position_opened_) {
      // Entry: rate must be above threshold, settlement within entry window
      if (std::abs(funding_rate_.rate) > kConfig.fundingRateThreshold &&
          time_to_settlement > 0 &&
          time_to_settlement < ENTRY_BEFORE_SETTLEMENT_MS) {
        NOTICE(instId_ << " entry signal: rate=" << funding_rate_.rate
                       << ", time_to_settlement=" << time_to_settlement / 1000
                       << "s");
        if (OpenPosition()) {
          position_opened_ = true;
        }
      } else {
        DEBUG(instId_ << " waiting: rate=" << funding_rate_.rate
                      << " threshold=" << kConfig.fundingRateThreshold
                      << " time_to_settlement=" << time_to_settlement / 1000
                      << "s");
      }
    } else if (in_position && position_opened_) {
      // Exit: wait until settlement has passed + buffer
      if (now_ms > settlement_time_ + EXIT_AFTER_SETTLEMENT_MS) {
        NOTICE(instId_ << " funding collected, closing position");
        if (ClosePosition()) {
          position_opened_ = false;
          break;  // one round done; manager will restart for next opportunity
        }
      } else {
        int64_t remain =
            static_cast<int64_t>(settlement_time_ + EXIT_AFTER_SETTLEMENT_MS) -
            static_cast<int64_t>(now_ms);
        DEBUG(instId_ << " holding, settlement in " << remain / 1000 << "s");
      }
    } else if (!in_position && position_opened_) {
      // Position was closed externally (e.g. liquidation or manual close)
      NOTICE(instId_ << " position closed externally");
      position_opened_ = false;
      break;
    }

    SLEEP_MS(STRATEGY_POLL_INTERVAL_MS);
  }
  DEBUG("Strategy stop running " << instId_);
}
