#ifndef BITGET_H
#define BITGET_H

#include <curl/curl.h>

#include <map>
#include <mutex>
#include <string>

#include "data.h"
#include "util.h"

class Bitget {
 public:
  Bitget() = default;
  Bitget(const std::string& instId);
  ~Bitget();
  void init();
  std::string getInstId() { return symbol_.symbol; };
  int precision() { return safeStoi(symbol_.pricePlace); }
  std::string sendRequest(const std::string& requestPath,
                          const std::string& method,
                          const std::string& query = "",
                          const std::string& body = "");
  bool symbols();
  bool setLeverage(const int leverage);
  bool placeOrder(Order& order);           // limit maker order
  bool placeMarketOrder(Order& order);     // market taker order (open)
  bool placeMarginOrder(MarginOrder& order);  // crossed margin only
  bool closePosition();                    // market close current position
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

 private:
  std::mutex curl_mtx_;
  Symbol symbol_;

  static std::string baseUrl_;
  static std::string apiKey_;
  static std::string secretKey_;
  static std::string passphrase_;
  static std::mutex curl_smtx_;
};

#endif  // BITGET_H
