#ifndef _DATA_H
#define _DATA_H

#include <cstdint>
#include <string>

#include "defines.h"

struct Config {
  float input{1.0f};
  float lever;
  float fundingRateThreshold;

  std::string apiKey;
  std::string secretKey;
  std::string passphrase;

  std::string logName;
  std::string logSize;
  std::string logLevel;

  std::string barkServer;
};

extern Config kConfig;

struct Ticker {
  std::string symbol;
  float lastPr;
  float bidPr;
  float askPr;
};

struct Order {
  float size{0.0};
  float price{0.0};
  std::string symbol;
  std::string orderId;
  std::string side;
  std::string status;
  std::string tradeSide;
  std::string reduceOnly;
};

struct Position {
  std::string symbol;
  std::string holdSide;
  float available{0.0};
  float locked{0.0};
  float total{0.0};
};

struct Symbol {
  std::string symbol;
  std::string minTradeNum;
  std::string maxLever;
  std::string volumePlace;
  std::string pricePlace;
  std::string sizeMultiplier;
};

struct Saving {
  std::string productId;
  std::string orderId;
  std::string productCoin;
  std::string holdAmount;
  std::string periodType;
};

// Funding rate info for a perpetual contract
struct FundingRate {
  std::string symbol;
  float rate{0.0};           // current funding rate (positive = longs pay shorts)
  int fundingRateInterval{0};  // funding interval in hours
  uint64_t nextFundingTime{0}; // next settlement timestamp in ms
};

#endif
