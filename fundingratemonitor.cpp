#include "fundingratemonitor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <map>
#include <utility>

#include "bitget.h"
#include "nlohmann/json.hpp"
#include "tracer.h"
#include "util.h"

FundingRateMonitor::FundingRateMonitor(int intervalSeconds) {
  setIntervalSeconds(intervalSeconds);
}

FundingRateMonitor::~FundingRateMonitor() { stop(); }

void FundingRateMonitor::start() {
  thread_ = std::make_shared<Poco::Thread>();
  thread_->setName("FRM");
  if (!thread_running_ && thread_) {
    thread_->start(*this);
    thread_running_ = true;
  }
}

void FundingRateMonitor::stop() {
  thread_running_ = false;
  if (thread_) {
    thread_->join();
    thread_ = nullptr;
  }
}

void FundingRateMonitor::setIntervalSeconds(int intervalSeconds) {
  if (intervalSeconds <= 0) {
    intervalSeconds = 60;
  }

  {
    std::lock_guard<std::mutex> lock(stateMutex_);
    intervalSeconds_ = intervalSeconds;
  }
}

int FundingRateMonitor::getIntervalSeconds() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return intervalSeconds_;
}

bool FundingRateMonitor::fetchFundingRateBySymbol(const std::string& symbol,
                                                  FundingRate& fr) const {
  const std::string requestPath = "/api/v2/mix/market/current-fund-rate";
  const std::string queryString =
      "?symbol=" + symbol + "&productType=usdt-futures";
  const std::string response =
      Bitget::sendRequestStatic(requestPath, "GET", queryString);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("fetchFundingRateBySymbol API error: " << code
                                                   << ", symbol=" << symbol);
      return false;
    }

    nlohmann::json data = j["data"];
    if (data.is_array()) {
      if (data.empty()) {
        return false;
      }
      data = data[0];
    }

    fr.symbol = symbol;
    if (data.contains("fundingRate") && !data["fundingRate"].is_null()) {
      fr.rate = safeStof(data["fundingRate"]);
    }
    if (data.contains("fundingRateInterval") &&
        !data["fundingRateInterval"].is_null()) {
      fr.fundingRateInterval = safeStoi(data["fundingRateInterval"]);
    }

    if (data.contains("nextUpdate") && !data["nextUpdate"].is_null()) {
      fr.nextFundingTime = safeStoll(data["nextUpdate"]);
    } else if (data.contains("fundingTime") && !data["fundingTime"].is_null()) {
      fr.nextFundingTime = safeStoll(data["fundingTime"]);
    }

    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("fetchFundingRateBySymbol parse error: "
          << e.what() << ", symbol=" << symbol << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("fetchFundingRateBySymbol error: " << e.what()
                                             << ", symbol=" << symbol);
  }

  return false;
}

bool FundingRateMonitor::refreshOnce() {
  Bitget client;
  std::map<std::string, FundingRate> tickerRates;
  if (!client.tickers(tickerRates)) {
    ERROR("refreshOnce failed: no ticker funding rates fetched");
    return false;
  }

  std::vector<FundingRate> snapshot;
  snapshot.reserve(tickerRates.size());

  for (auto& item : tickerRates) {
    if (!Bitget::crossedMarginSymbolSupported(item.first)) {
      DEBUG("FundingRateMonitor: " << item.first
                                   << " does not support cross margin, skip");
      continue;
    }
    FundingRate fr = item.second;
    if (fetchFundingRateBySymbol(item.first, fr)) {
      DEBUG("FundingRateMonitor: "
            << fr.symbol << " rate=" << fr.rate << " nextFunding=" << std::fixed
            << std::setprecision(1)
            << (static_cast<int64_t>(fr.nextFundingTime) -
                static_cast<int64_t>(getCurrentTimeMs())) /
                   3600000.0
            << "h");
      snapshot.push_back(fr);
    }
  }

  if (snapshot.empty()) {
    ERROR("refreshOnce failed: no funding rates fetched");
    return false;
  }

  size_t refreshedCount = 0;
  {
    std::lock_guard<std::mutex> lock(dataMutex_);
    refreshedCount = snapshot.size();
    fundingRates_ = std::move(snapshot);
    lastRefreshTimeMs_ = getCurrentTimeMs();
  }

  NOTICE("FundingRateMonitor refreshed " << refreshedCount << " symbols");
  return true;
}

bool FundingRateMonitor::getMaxAbsFundingRate(FundingRate& out) const {
  std::lock_guard<std::mutex> lock(dataMutex_);
  if (fundingRates_.empty()) {
    return false;
  }

  auto it = std::max_element(fundingRates_.begin(), fundingRates_.end(),
                             [](const FundingRate& a, const FundingRate& b) {
                               return std::fabs(a.rate) < std::fabs(b.rate);
                             });

  out = *it;
  return true;
}

std::vector<FundingRate> FundingRateMonitor::getAllFundingRates() const {
  std::lock_guard<std::mutex> lock(dataMutex_);
  return fundingRates_;
}

uint64_t FundingRateMonitor::getLastRefreshTimeMs() const {
  std::lock_guard<std::mutex> lock(dataMutex_);
  return lastRefreshTimeMs_;
}

void FundingRateMonitor::run() {
  while (thread_running_) {
    refreshOnce();

    int intervalSeconds = 60;
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      intervalSeconds = intervalSeconds_;
    }

    SLEEP_MS(intervalSeconds * 1000);
  }
}
