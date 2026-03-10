#include "strategy.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "tracer.h"
#include "util.h"

Strategy::Strategy(std::shared_ptr<Bitget> client) : client_(client) { init(); }

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

void Strategy::init() {
  instId_ = client_->getInstId();
  NOTICE("Strategy initialized for " << instId_);
}

bool Strategy::updateFundingRate() {
  return client_->fundingRate(funding_rate_);
}

bool Strategy::openPosition() {
  FundingRate currentFunding;
  if (!client_->fundingRate(currentFunding)) {
    ERROR("OpenPosition: failed to get funding rate for " << instId_);
    return false;
  }
  if (std::fabs(currentFunding.rate) < FLOAT_EPSILON) {
    ERROR("OpenPosition: funding rate is zero, skip open for " << instId_);
    return false;
  }

  bool futuresShort = currentFunding.rate > 0;
  std::string futuresSide = futuresShort ? "sell" : "buy";
  std::string marginSide = futuresShort ? "buy" : "sell";

  Ticker ticker;
  if (!client_->tickers(ticker) || ticker.lastPr <= 0) {
    ERROR("OpenPosition: failed to get ticker for " << instId_);
    return false;
  }

  float minBaseSize = safeStof(client_->getSymbol().minTradeNum);
  if (minBaseSize <= 0) {
    ERROR("OpenPosition: invalid minTradeNum for " << instId_);
    return false;
  }

  // sizeMultiplier is the minimum lot size increment on Bitget futures.
  // Each contract = 1 base token; size parameter must be a multiple of lotSize.
  float lotSize = safeStof(client_->getSymbol().sizeMultiplier);
  if (lotSize <= 0) lotSize = 1.0f;

  const int leverage = static_cast<int>(std::max(1.0f, kConfig.lever));
  const float targetTokenQtyRaw =
      kConfig.openPrincipal * leverage / ticker.lastPr;
  int hedgeTokenQty = static_cast<int>(std::floor(targetTokenQtyRaw));
  if (hedgeTokenQty <= 0) {
    ERROR("OpenPosition: principal too small, openPrincipal="
          << kConfig.openPrincipal << " leverage=" << leverage
          << " price=" << ticker.lastPr);
    return false;
  }

  // Round down to nearest lot size boundary.
  int contractCount = static_cast<int>(
      std::floor(static_cast<float>(hedgeTokenQty) / lotSize)) *
      static_cast<int>(lotSize);
  if (contractCount <= 0) {
    ERROR("OpenPosition: token qty too small for 1 lot, tokenQty="
          << hedgeTokenQty << " lotSize=" << lotSize);
    return false;
  }
  hedgeTokenQty = contractCount;  // align margin qty to lot boundary

  MarginOrder marginOrder;
  marginOrder.symbol = instId_;
  marginOrder.side = marginSide;
  marginOrder.orderType = "market";
  marginOrder.loanType = "autoLoan";
  if (marginSide == "buy") {
    marginOrder.quoteSize =
        safeFtos(static_cast<float>(hedgeTokenQty) *
                     (ticker.askPr > 0 ? ticker.askPr : ticker.lastPr),
                 client_->precision());
  } else {
    marginOrder.baseSize = safeFtos(static_cast<float>(hedgeTokenQty),
                                    safeStoi(client_->getSymbol().volumePlace));
  }

  Order futuresOrder;
  futuresOrder.symbol = instId_;
  futuresOrder.side = futuresSide;
  futuresOrder.size = static_cast<float>(contractCount);

  bool marginOk = client_->placeMarginOrder(marginOrder);
  bool futuresOk = client_->placeMarketOrder(futuresOrder);

  NOTICE("OpenPosition result inst="
         << instId_ << " marginOk=" << marginOk << " futuresOk=" << futuresOk
         << " futuresSide=" << futuresSide << " marginSide=" << marginSide
         << " contracts=" << contractCount << " tokenQty=" << hedgeTokenQty
         << " fundingRate=" << currentFunding.rate);

  return marginOk && futuresOk;
}

bool Strategy::closePosition() {
  bool marginOk = client_->closeCrossedMarginPosition();
  bool futuresOk = client_->closeFuturesPosition();
  NOTICE("ClosePosition result inst=" << instId_ << " marginOk=" << marginOk
                                      << " futuresOk=" << futuresOk);
  if (!(marginOk && futuresOk)) {
    return false;
  }
  return true;
}

bool Strategy::hasPosition() {
  if (!client_->singlePosition(position_)) return false;
  return position_.total > 0;
}

void Strategy::run() {
  DEBUG("Strategy start running " << instId_);
  while (thread_running_) {
    if (!updateFundingRate()) {
      SLEEP_MS(STRATEGY_POLL_INTERVAL_MS);
      continue;
    }

    bool in_position = hasPosition();
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
        if (openPosition()) {
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
        if (closePosition()) {
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
