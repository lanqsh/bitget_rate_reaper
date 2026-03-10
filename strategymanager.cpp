#include "strategymanager.h"

#include <cmath>
#include <iomanip>
#include <vector>

#include "bitget.h"
#include "data.h"
#include "defines.h"
#include "util.h"

StrategyManager::StrategyManager(std::shared_ptr<FundingRateMonitor> monitor)
    : monitor_(monitor) {}

StrategyManager::~StrategyManager() { stop(); }

void StrategyManager::start() {
  thread_ = std::make_shared<Poco::Thread>();
  thread_->setName("MGR");
  if (!thread_running_ && thread_) {
    thread_->start(*this);
    thread_running_ = true;
  }
}

void StrategyManager::stop() {
  thread_running_ = false;
  {
    std::lock_guard<std::mutex> lock(strategies_mtx_);
    strategy_.reset();
  }
  if (thread_) {
    thread_->join();
    thread_ = nullptr;
  }
}

void StrategyManager::run() {
  DEBUG("StrategyManager start running");
  recoverPosition();
  while (thread_running_) {
    {
      std::lock_guard<std::mutex> lock(strategies_mtx_);

      if (!strategy_) {
        FundingRate best;
        if (monitor_->getMaxAbsFundingRate(best) &&
            std::fabs(best.rate) >= kConfig.fundingRateThreshold) {
          uint64_t now_ms = getCurrentTimeMs();
          int64_t time_to_settlement =
              static_cast<int64_t>(best.nextFundingTime) -
              static_cast<int64_t>(now_ms);
          if (time_to_settlement > 0 &&
              time_to_settlement < ENTRY_BEFORE_SETTLEMENT_MS) {
            std::string baseCoin = best.symbol;
            const std::string suffix = MARGIN_COIN;
            if (baseCoin.size() > suffix.size() &&
                baseCoin.substr(baseCoin.size() - suffix.size()) == suffix) {
              baseCoin = baseCoin.substr(0, baseCoin.size() - suffix.size());
            }
            NOTICE("StrategyManager: launching strategy for "
                   << best.symbol << " rate=" << best.rate
                   << " time_to_settlement=" << time_to_settlement / 1000
                   << "s");
            auto client = std::make_shared<Bitget>(baseCoin);
            strategy_ = std::make_shared<Strategy>(client);
            if (strategy_->openPosition()) {
              position_opened_ = true;
              settlement_time_ = best.nextFundingTime;
            } else {
              strategy_.reset();
            }
          } else {
            DEBUG("StrategyManager: best symbol=" << best.symbol
                  << " rate=" << best.rate * 100.0f << "%"
                  << " time_to_settlement="
                  << std::fixed << std::setprecision(1)
                  << time_to_settlement / 3600000.0 << "h"
                  << " not in entry window");
          }
        } else {
          DEBUG("StrategyManager: no qualifying funding rate");
        }
      } else {
        bool in_position = strategy_->hasPosition();
        uint64_t now_ms = getCurrentTimeMs();
        if (in_position && position_opened_) {
          if (now_ms > settlement_time_ + EXIT_AFTER_SETTLEMENT_MS) {
            NOTICE("StrategyManager: funding collected, closing "
                   << strategy_->getInstId());
            if (strategy_->closePosition()) {
              position_opened_ = false;
              strategy_.reset();
            }
          } else if (strategy_->checkStopLoss()) {
            NOTICE("StrategyManager: stop-loss triggered, closing "
                   << strategy_->getInstId());
            if (strategy_->closePosition()) {
              position_opened_ = false;
              strategy_.reset();
            }
          } else {
            int64_t remain = static_cast<int64_t>(settlement_time_ +
                                                  EXIT_AFTER_SETTLEMENT_MS) -
                             static_cast<int64_t>(now_ms);
            DEBUG("StrategyManager: holding " << strategy_->getInstId()
                  << " settle in " << remain / 1000 << "s");
          }
        } else if (!in_position && position_opened_) {
          NOTICE("StrategyManager: position closed externally for "
                 << strategy_->getInstId());
          position_opened_ = false;
          strategy_.reset();
        }
      }
    }

    SLEEP_MS(STRATEGY_POLL_INTERVAL_MS);
  }
  DEBUG("StrategyManager stop running");
}

void StrategyManager::recoverPosition() {
  std::vector<Position> positions;
  if (!Bitget::allFuturesPositions(positions) || positions.empty()) {
    NOTICE("StrategyManager: no existing futures positions on startup");
    return;
  }

  const Position& pos = positions[0];
  std::string symbol = pos.symbol;
  std::string baseCoin = symbol;
  const std::string suffix = MARGIN_COIN;
  if (baseCoin.size() > suffix.size() &&
      baseCoin.substr(baseCoin.size() - suffix.size()) == suffix) {
    baseCoin = baseCoin.substr(0, baseCoin.size() - suffix.size());
  }

  CrossedMarginAsset asset;
  bool hasMarginPos = Bitget::crossedMarginAsset(baseCoin, asset) &&
                      std::fabs(asset.borrow) > FLOAT_EPSILON;

  NOTICE("StrategyManager: recovered position symbol=" << symbol
         << " total=" << pos.total
         << " holdSide=" << pos.holdSide
         << " marginBorrow=" << asset.borrow
         << " hasMarginPos=" << hasMarginPos);

  auto client = std::make_shared<Bitget>(baseCoin);
  std::lock_guard<std::mutex> lock(strategies_mtx_);
  strategy_ = std::make_shared<Strategy>(client);
  position_opened_ = true;
  settlement_time_ = 0;
}
