#ifndef _STRATEGY_H
#define _STRATEGY_H

#include <memory>
#include <string>

#include "bitget.h"
#include "tracer.h"

class Strategy {
 public:
  explicit Strategy(std::shared_ptr<Bitget> client);
  bool openPosition();
  bool closePosition();
  bool hasPosition();
  bool updateFundingRate();
  bool checkStopLoss();
  std::string getInstId() { return instId_; }
  float getFundingRate() { return funding_rate_.rate; }
  uint64_t getNextFundingTime() { return funding_rate_.nextFundingTime; }

 private:
  void init();

 private:
  std::string instId_;
  std::shared_ptr<Bitget> client_;
  FundingRate funding_rate_;
  Position position_;
  float entry_price_{0.0f};
};

#endif