#ifndef BITGET_H
#define BITGET_H

#include <curl/curl.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "data.h"
#include "util.h"

class Bitget {
 public:
  Bitget() = default;
  Bitget(const std::string& instId);
  ~Bitget();
  void init();
  std::string getBaseCoin() const { return symbol_.symbol; }
  std::string getInstId() const { return symbol_.symbol + MARGIN_COIN; }
  int precision() { return safeStoi(symbol_.pricePlace); }
  std::string sendRequest(const std::string& requestPath,
                          const std::string& method,
                          const std::string& query = "",
                          const std::string& body = "");
  bool symbols();
  bool setLeverage(const int leverage);
  bool placeOrder(Order& order);
  bool placeMarketOrder(Order& order);
  bool placeMarginOrder(MarginOrder& order);
  bool closeFuturesPosition();
  bool closeCrossedMarginPosition(const std::string& coin = "");
  bool cancelOrder(const std::string& orderId);
  bool tickers(Ticker& ticker);
  bool tickers(std::map<std::string, FundingRate>& fundingRates);
  bool fundingRate(FundingRate& fr);
  bool singlePosition(Position& position);
  const Symbol& getSymbol() const { return symbol_; }

  static void setApiParam();
  static bool assets(float& available);
  static std::string sendRequestStatic(const std::string& requestPath,
                                       const std::string& method,
                                       const std::string& query = "",
                                       const std::string& body = "");
  static bool savingsAssets(Saving& saving);
  static bool savingsSubscribe(Saving& saving);
  static bool savingsRedeem(Saving& saving);
  static bool transfer(const std::string& amount, const std::string& src,
                       const std::string& dst);
  static bool crossedMarginAsset(const std::string& coin,
                                 CrossedMarginAsset& asset);
  bool crossedMarginRepay(const std::string& coin, float amount);
  bool flashRepay(const std::string& coin);
  static bool crossedMarginSymbolSupported(const std::string& symbol);
  static void crossedMarginRemoveFromWhitelist(const std::string& symbol);
  static bool crossedMarginInterestRateAndLimit(const std::string& coin,
                                                MarginInterestInfo& info);
  static bool allFuturesPositions(std::vector<Position>& positions);

 private:
  std::mutex curl_mtx_;
  CURL* curl_{nullptr};
  Symbol symbol_;

  static std::string baseUrl_;
  static std::string apiKey_;
  static std::string secretKey_;
  static std::string passphrase_;
  static std::mutex curl_smtx_;
  static CURL* curl_s_;

  static std::set<std::string> crossedMarginSymbols_;
  static bool crossedMarginSymbolsLoaded_;
  static std::mutex crossedMarginSymbolsMtx_;
};

#endif
