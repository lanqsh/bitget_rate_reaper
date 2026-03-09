#ifndef _STRATEGY_H
#define _STRATEGY_H

#include <memory>
#include <string>

#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "bitget.h"
#include "tracer.h"

// Funding rate arbitrage strategy for a single perpetual contract.
// Opens a position before funding settlement and closes after collecting.
class Strategy : public Poco::Runnable {
 public:
  Strategy(std::shared_ptr<Bitget> client);
  virtual ~Strategy();
  void run() override;

  void start();
  void stop();
  void wait();
  bool isRunning() { return thread_ && thread_->isRunning(); }
  std::string GetInstId() { return instId_; }
  float GetFundingRate() { return funding_rate_.rate; }

 private:
  void Init();
  bool UpdateFundingRate();
  bool OpenPosition();
  bool ClosePosition();
  bool HasPosition();

 private:
  bool thread_running_{false};

  std::string instId_;
  std::shared_ptr<Poco::Thread> thread_;
  std::shared_ptr<Bitget> client_;

  FundingRate funding_rate_;
  Position position_;
  bool position_opened_{false};
  uint64_t settlement_time_{0};  // next settlement timestamp at position open time
};

#endif