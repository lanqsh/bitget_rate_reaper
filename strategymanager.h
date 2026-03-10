#ifndef _STRATEGY_MANAGER_H
#define _STRATEGY_MANAGER_H

#include <memory>
#include <mutex>
#include <string>

#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "fundingratemonitor.h"
#include "strategy.h"
#include "tracer.h"

class StrategyManager : public Poco::Runnable {
 public:
  explicit StrategyManager(std::shared_ptr<FundingRateMonitor> monitor);
  virtual ~StrategyManager();

  void run() override;
  void start();
  void stop();

  bool isRunning() { return thread_ && thread_->isRunning(); }

 private:
  void recoverPosition();

 private:
  bool thread_running_{false};
  bool position_opened_{false};
  uint64_t settlement_time_{0};
  std::shared_ptr<Poco::Thread> thread_;
  std::shared_ptr<FundingRateMonitor> monitor_;
  std::mutex strategies_mtx_;
  std::shared_ptr<Strategy> strategy_;
};

#endif
