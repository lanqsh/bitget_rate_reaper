#ifndef _FUNDING_RATE_MONITOR_H
#define _FUNDING_RATE_MONITOR_H

#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "data.h"

class FundingRateMonitor : public Poco::Runnable {
 public:
  explicit FundingRateMonitor(int intervalSeconds = 60);
  ~FundingRateMonitor();

  void run() override;
  void start();
  void stop();
  void setIntervalSeconds(int intervalSeconds);
  int getIntervalSeconds() const;

  bool refreshOnce();
  bool getMaxAbsFundingRate(FundingRate& out) const;
  std::vector<FundingRate> getAllFundingRates() const;
  uint64_t getLastRefreshTimeMs() const;

 private:
  bool fetchFundingRateBySymbol(const std::string& symbol, FundingRate& fr) const;

 private:
  bool thread_running_{false};
  std::shared_ptr<Poco::Thread> thread_;
  mutable std::mutex stateMutex_;
  int intervalSeconds_{60};

  mutable std::mutex dataMutex_;
  std::vector<FundingRate> fundingRates_;
  uint64_t lastRefreshTimeMs_{0};
};

#endif
