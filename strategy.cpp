#include "strategy.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "tracer.h"
#include "util.h"

Strategy::Strategy(std::shared_ptr<Bitget> client) : client_(client) { init(); }

void Strategy::init() {
  instId_ = client_->getInstId();
  NOTICE("Strategy initialized for " << instId_);
}

bool Strategy::updateFundingRate() {
  return client_->fundingRate(funding_rate_);
}

bool Strategy::hasPosition() {
  if (!client_->singlePosition(position_)) return false;
  return position_.total > 0;
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

  int contractCount = static_cast<int>(
      std::floor(static_cast<float>(hedgeTokenQty) / lotSize)) *
      static_cast<int>(lotSize);
  if (contractCount <= 0) {
    ERROR("OpenPosition: token qty too small for 1 lot, tokenQty="
          << hedgeTokenQty << " lotSize=" << lotSize);
    return false;
  }
  hedgeTokenQty = contractCount;

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
  if (!marginOk) {
    ERROR("OpenPosition: margin order failed, skip futures for " << instId_);
    return false;
  }

  bool futuresOk = client_->placeMarketOrder(futuresOrder);

  NOTICE("OpenPosition result inst="
         << instId_ << " marginOk=" << marginOk << " futuresOk=" << futuresOk
         << " futuresSide=" << futuresSide << " marginSide=" << marginSide
         << " contracts=" << contractCount << " tokenQty=" << hedgeTokenQty
         << " fundingRate=" << currentFunding.rate);

  if (marginOk && futuresOk) {
    entry_price_ = ticker.lastPr;
    std::string msg = "OpenPosition " + instId_ +
                      " side=" + futuresSide +
                      " price=" + safeFtos(ticker.lastPr, client_->precision()) +
                      " fundingRate=" + safeFtos(currentFunding.rate * 100.0f, 4) + "%";
    sendMessage(msg, true);
    return true;
  }

  // Futures failed after margin succeeded: roll back margin leg
  NOTICE("OpenPosition: futures failed, closing margin leg for " << instId_);
  client_->closeCrossedMarginPosition();
  return false;
}

bool Strategy::closePosition() {
  bool marginOk = client_->flashRepay();
  if (!marginOk) {
    NOTICE("ClosePosition: flashRepay failed, falling back to closeCrossedMarginPosition for " << instId_);
    marginOk = client_->closeCrossedMarginPosition();
  }
  bool futuresOk = client_->closeFuturesPosition();
  NOTICE("ClosePosition result inst=" << instId_ << " marginOk=" << marginOk
                                      << " futuresOk=" << futuresOk);
  if (!(marginOk && futuresOk)) {
    return false;
  }
  return true;
}

bool Strategy::checkStopLoss() {
  if (entry_price_ <= 0) return false;

  Ticker ticker;
  if (!client_->tickers(ticker) || ticker.lastPr <= 0) {
    ERROR("checkStopLoss: failed to get ticker for " << instId_);
    return false;
  }

  float deviation = std::fabs(ticker.lastPr - entry_price_) / entry_price_;
  if (deviation >= STOP_LOSS_RATIO) {
    NOTICE("checkStopLoss: price deviation " << deviation * 100.0f
           << "% >= " << STOP_LOSS_RATIO * 100.0f
           << "%, entry=" << entry_price_
           << " current=" << ticker.lastPr
           << " inst=" << instId_ << ", closing position");
    return true;
  }

  DEBUG("checkStopLoss: inst=" << instId_
        << " entry=" << entry_price_
        << " current=" << ticker.lastPr
        << " deviation=" << deviation * 100.0f << "%");
  return false;
}
