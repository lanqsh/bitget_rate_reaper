#ifndef _DATA_H
#define _DATA_H

#include <cstdint>
#include <string>

#include "defines.h"

struct Config {
  float openPrincipal{1.0f};
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

struct MarginOrder {
  std::string symbol;
  std::string side;
  std::string orderType;
  std::string loanType;
  std::string force;
  std::string price;
  std::string baseSize;
  std::string quoteSize;
  std::string clientOid;
  std::string stpMode;
  std::string orderId;
};

struct CrossedMarginAsset {
  std::string coin;
  float totalAmount{0.0};
  float available{0.0};
  float frozen{0.0};
  float borrow{0.0};
  float interest{0.0};
  float net{0.0};
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

struct FundingRate {
  std::string symbol;
  float rate{0.0};
  int fundingRateInterval{0};
  uint64_t nextFundingTime{0};
};

#endif
