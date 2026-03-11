#include "bitget.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/Timestamp.h"
#include "Poco/Timezone.h"
#include "nlohmann/json.hpp"
#include "tracer.h"
#include "util.h"

std::string Bitget::baseUrl_;
std::string Bitget::apiKey_;
std::string Bitget::secretKey_;
std::string Bitget::passphrase_;

std::mutex Bitget::curl_smtx_;
CURL* Bitget::curl_s_ = nullptr;

std::set<std::string> Bitget::crossedMarginSymbols_;
bool Bitget::crossedMarginSymbolsLoaded_ = false;
std::mutex Bitget::crossedMarginSymbolsMtx_;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  size_t totalSize = size * nmemb;
  std::string* str = (std::string*)userp;
  str->append((char*)contents, totalSize);
  return totalSize;
}

Bitget::Bitget(const std::string& instId) {
  symbol_.symbol = instId;
  init();
}

Bitget::~Bitget() {
  if (curl_) {
    curl_easy_cleanup(curl_);
    curl_ = nullptr;
  }
}

void Bitget::init() {
  symbols();
  setLeverage(kConfig.lever);
}

void Bitget::setApiParam() {
  apiKey_ = kConfig.apiKey;
  secretKey_ = kConfig.secretKey;
  passphrase_ = kConfig.passphrase;
  baseUrl_ = "https://api.bitget.com";
}

std::string Bitget::sendRequest(const std::string& requestPath,
                                const std::string& method,
                                const std::string& query,
                                const std::string& body) {
  INFO_("api", "===sendRequest start===\n"
                   << method << " " << requestPath << ", query: " << query
                   << ", body: " << body);
  std::lock_guard<std::mutex> lock(curl_mtx_);
  if (!curl_) curl_ = curl_easy_init();
  CURLcode res;
  std::string readBuffer;
  if (curl_) {
    curl_easy_reset(curl_);
    std::string timestamp = getTimestampMs();

    std::string prehash = timestamp + method + requestPath + query + body;
    std::string hash = hmac_sha256(secretKey_, prehash);
    std::string signature = base64_encode(hash);

    std::string url = baseUrl_ + requestPath;
    if (!query.empty()) url += query;
    INFO_("api", url);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("ACCESS-KEY: " + apiKey_).c_str());
    headers = curl_slist_append(headers, ("ACCESS-SIGN: " + signature).c_str());
    headers =
        curl_slist_append(headers, ("ACCESS-TIMESTAMP: " + timestamp).c_str());
    headers = curl_slist_append(headers,
                                ("ACCESS-PASSPHRASE: " + passphrase_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "locale: en-US");

    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (method == "POST") {
      curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    }
    if (method == "DELETE") {
      curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
      ERROR_("api", "curl_easy_perform() failed: " << curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
  }

  try {
    nlohmann::json jsonData = nlohmann::json::parse(readBuffer);
    INFO_("api", "Response: " << jsonData.dump(4));
  } catch (const nlohmann::json::exception& e) {
    ERROR_("api", "Error parsing JSON: " << e.what());
  }
  INFO_("api", "===sendRequest end===\n");

  return readBuffer;
}

std::string Bitget::sendRequestStatic(const std::string& requestPath,
                                      const std::string& method,
                                      const std::string& query,
                                      const std::string& body) {
  DEBUG_("api", "===sendRequest start===\n"
                    << method << " " << requestPath << ", query: " << query
                    << ", body: " << body);
  std::lock_guard<std::mutex> lock(curl_smtx_);
  if (!curl_s_) curl_s_ = curl_easy_init();
  CURLcode res;
  std::string readBuffer;
  if (curl_s_) {
    curl_easy_reset(curl_s_);
    std::string timestamp = getTimestampMs();

    std::string prehash = timestamp + method + requestPath + query + body;
    std::string hash = hmac_sha256(secretKey_, prehash);
    std::string signature = base64_encode(hash);

    std::string url = baseUrl_ + requestPath;
    if (!query.empty()) url += query;
    DEBUG_("api", url);
    DEBUG_("api", apiKey_);
    DEBUG_("api", signature);
    DEBUG_("api", timestamp);
    DEBUG_("api", passphrase_);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("ACCESS-KEY: " + apiKey_).c_str());
    headers = curl_slist_append(headers, ("ACCESS-SIGN: " + signature).c_str());
    headers =
        curl_slist_append(headers, ("ACCESS-TIMESTAMP: " + timestamp).c_str());
    headers = curl_slist_append(headers,
                                ("ACCESS-PASSPHRASE: " + passphrase_).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "locale: en-US");

    curl_easy_setopt(curl_s_, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_s_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_s_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_s_, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl_s_, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (method == "POST") {
      curl_easy_setopt(curl_s_, CURLOPT_POSTFIELDS, body.c_str());
    }
    if (method == "DELETE") {
      curl_easy_setopt(curl_s_, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    res = curl_easy_perform(curl_s_);
    if (res != CURLE_OK) {
      ERROR_("api", "curl_easy_perform() failed: " << curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
  }

  try {
    nlohmann::json jsonData = nlohmann::json::parse(readBuffer);
    DEBUG_("api", "Response: \n" << jsonData.dump(4));
  } catch (const nlohmann::json::exception& e) {
    ERROR_("api", "Error parsing JSON: " << e.what());
  }
  DEBUG_("api", "===sendRequest end===\n");

  return readBuffer;
}

bool Bitget::symbols() {
  std::string requestPath = "/api/v2/mix/market/contracts";
  std::string queryString = "?productType=usdt-futures&symbol=" + getInstId();
  auto response = sendRequest(requestPath, "GET", queryString);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    if (j["data"].size() != 1) return false;
    for (const auto& item : j["data"]) {
      symbol_.minTradeNum = item["minTradeNum"];
      symbol_.maxLever = item["maxLever"];
      symbol_.volumePlace = item["volumePlace"];
      symbol_.pricePlace = item["pricePlace"];
      symbol_.sizeMultiplier = item["sizeMultiplier"];
    }

    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("symbols error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::setLeverage(const int leverage) {
  std::string requestPath = "/api/v2/mix/account/set-leverage";
  std::string queryString;
  std::string queryBody;
  std::string str_leverage = std::to_string(leverage);
  if (leverage > safeStoi(symbol_.maxLever)) {
    str_leverage = symbol_.maxLever;
  }

  nlohmann::json j;

  j["symbol"] = getInstId();
  j["productType"] = "USDT-FUTURES";
  j["marginCoin"] = "USDT";
  j["leverage"] = str_leverage;

  queryBody = j.dump();

  auto response = sendRequest(requestPath, "POST", queryString, queryBody);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("placeOrder error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
  return false;
}

bool Bitget::placeOrder(Order& order) {
  std::string requestPath = "/api/v2/mix/order/place-order";
  std::string queryString;
  std::string queryBody;

  nlohmann::json j;

  j["symbol"] = getInstId();
  j["side"] = order.side;
  j["productType"] = "USDT-FUTURES";
  j["marginMode"] = "crossed";
  j["marginCoin"] = "USDT";
  j["tradeSide"] = "open";
  j["orderType"] = "limit";
  j["force"] = "post_only";

  j["price"] = adjustDecimalPlaces(order.price, symbol_.pricePlace);
  j["size"] = adjustDecimalPlaces(order.size, symbol_.volumePlace);

  queryBody = j.dump();

  auto response = sendRequest(requestPath, "POST", queryString, queryBody);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    order.orderId = j["data"]["orderId"];
    if (order.orderId.empty()) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("placeOrder error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::placeMarketOrder(Order& order) {
  std::string requestPath = "/api/v2/mix/order/place-order";
  nlohmann::json j;
  j["symbol"] = getInstId();
  j["side"] = order.side;
  j["productType"] = "USDT-FUTURES";
  j["marginMode"] = "crossed";
  j["marginCoin"] = "USDT";
  j["tradeSide"] = "open";
  j["orderType"] = "market";
  j["size"] = adjustDecimalPlaces(order.size, symbol_.volumePlace);

  auto response = sendRequest(requestPath, "POST", "", j.dump());
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    order.orderId = j["data"]["orderId"];
    return !order.orderId.empty();
  } catch (const nlohmann::json::exception& e) {
    ERROR("placeMarketOrder error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
  return false;
}

bool Bitget::placeMarginOrder(MarginOrder& order) {
  std::string symbol = order.symbol.empty() ? getInstId() : order.symbol;
  if (symbol.empty()) {
    ERROR("placeMarginOrder: symbol is required");
    return false;
  }
  if (order.side.empty()) {
    ERROR("placeMarginOrder: side is required");
    return false;
  }
  if (order.orderType.empty()) {
    ERROR("placeMarginOrder: orderType is required");
    return false;
  }
  if (order.loanType.empty()) {
    ERROR("placeMarginOrder: loanType is required");
    return false;
  }

  const bool is_limit = (order.orderType == "limit");
  const bool is_market = (order.orderType == "market");
  if (!is_limit && !is_market) {
    ERROR("placeMarginOrder: orderType must be limit or market");
    return false;
  }

  if (is_limit) {
    if (order.price.empty()) {
      ERROR("placeMarginOrder: price is required for limit orders");
      return false;
    }
    if (order.baseSize.empty()) {
      ERROR("placeMarginOrder: baseSize is required for limit orders");
      return false;
    }
  }

  if (is_market && order.side == "buy" && order.quoteSize.empty()) {
    ERROR("placeMarginOrder: quoteSize is required for market buy");
    return false;
  }
  if (is_market && order.side == "sell" && order.baseSize.empty()) {
    ERROR("placeMarginOrder: baseSize is required for market sell");
    return false;
  }

  std::string requestPath = "/api/v2/margin/crossed/place-order";

  nlohmann::json j;
  j["symbol"] = symbol;
  j["side"] = order.side;
  j["orderType"] = order.orderType;
  j["loanType"] = order.loanType;

  if (!order.clientOid.empty()) j["clientOid"] = order.clientOid;
  if (!order.stpMode.empty()) j["stpMode"] = order.stpMode;

  if (is_limit) {
    j["force"] = order.force.empty() ? "gtc" : order.force;
    j["price"] = order.price;
    j["baseSize"] = order.baseSize;
  } else {
    if (order.side == "buy") {
      j["quoteSize"] = order.quoteSize;
    } else {
      j["baseSize"] = order.baseSize;
    }
  }

  auto response = sendRequest(requestPath, "POST", "", j.dump());
  try {
    auto res = nlohmann::json::parse(response);
    std::string code = res["code"];
    if (code != API_SUCCESS) {
      if (code == "50004" || code == "50035") {
        NOTICE("placeMarginOrder: " << symbol
               << " removed from whitelist, code=" << code);
        std::lock_guard<std::mutex> wl(crossedMarginSymbolsMtx_);
        crossedMarginSymbols_.erase(symbol);
      } else {
        ERROR("API error: " << code << ", " << response);
      }
      return false;
    }

    if (res.contains("data") && res["data"].contains("orderId") &&
        !res["data"]["orderId"].is_null()) {
      order.orderId = res["data"]["orderId"];
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("placeMarginOrder error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
  return false;
}

bool Bitget::closeCrossedMarginPosition(const std::string& coin) {
  std::string baseCoin = coin.empty() ? getBaseCoin() : coin;
  if (baseCoin.empty()) {
    ERROR("closeCrossedMarginPosition: empty coin and symbol=" << getInstId());
    return false;
  }

  CrossedMarginAsset asset;
  if (!Bitget::crossedMarginAsset(baseCoin, asset)) {
    ERROR("closeCrossedMarginPosition: failed to query asset for " << baseCoin);
    return false;
  }

  const float netQty = asset.net;
  if (std::fabs(netQty) < FLOAT_EPSILON) {
    NOTICE("closeCrossedMarginPosition: " << baseCoin << " already flat");
    return true;
  }

  MarginOrder order;
  order.symbol = getInstId();
  order.orderType = "market";
  order.loanType = "autoRepay";

  if (netQty > 0) {
    float sellableQty = std::min(std::fabs(netQty), asset.available);
    if (sellableQty <= FLOAT_EPSILON) {
      ERROR("closeCrossedMarginPosition: no available qty to sell, net="
            << netQty << " available=" << asset.available);
      return false;
    }

    int places = safeStoi(symbol_.volumePlace);
    if (places < 0) places = 0;
    const float scale = std::pow(10.0f, static_cast<float>(places));
    sellableQty = std::floor(sellableQty * scale) / scale;
    if (sellableQty <= FLOAT_EPSILON) {
      ERROR("closeCrossedMarginPosition: sell qty too small after floor, qty="
            << sellableQty);
      return false;
    }

    order.side = "sell";
    order.baseSize = safeFtos(sellableQty, places);
  } else {
    Ticker ticker;
    if (!tickers(ticker) || ticker.lastPr <= 0) {
      ERROR("closeCrossedMarginPosition: failed to get ticker for "
            << getInstId());
      return false;
    }

    float referenceAsk = ticker.askPr > 0 ? ticker.askPr : ticker.lastPr;
    float borrowUsdt = std::fabs(netQty) * referenceAsk;
    order.side = "buy";

    if (borrowUsdt < 1.0f) {
      order.loanType = "normal";
      order.quoteSize = "1.0000";
    } else {
      order.loanType = "autoRepay";
      order.quoteSize = safeFtos(borrowUsdt * 1.015f, 4);
    }
  }

  bool ok = placeMarginOrder(order);
  NOTICE("closeCrossedMarginPosition result symbol="
         << getInstId() << " coin=" << baseCoin << " net=" << netQty
         << " side=" << order.side << " loanType=" << order.loanType
         << " ok=" << ok);

  if (ok && netQty < 0 && order.loanType == "normal") {
    float repayAmount = asset.borrow + asset.interest;
    if (repayAmount > FLOAT_EPSILON) {
      bool repaid = crossedMarginRepay(baseCoin, repayAmount);
      NOTICE("closeCrossedMarginPosition manual repay coin=" << baseCoin
             << " amount=" << repayAmount << " ok=" << repaid);
      ok = repaid;
    }
    return ok;
  }

  if (ok && netQty < 0) {
    CrossedMarginAsset assetAfter;
    if (Bitget::crossedMarginAsset(baseCoin, assetAfter) &&
        assetAfter.net < -FLOAT_EPSILON) {
      NOTICE("closeCrossedMarginPosition: residual short " << assetAfter.net
             << " " << baseCoin << ", retrying");
      Ticker ticker2;
      if (!tickers(ticker2) || ticker2.lastPr <= 0) return ok;
      float refAsk2 = ticker2.askPr > 0 ? ticker2.askPr : ticker2.lastPr;
      float residualUsdt = std::fabs(assetAfter.net) * refAsk2;
      if (residualUsdt < 1.0f) {
        MarginOrder retryOrder;
        retryOrder.symbol = getInstId();
        retryOrder.orderType = "market";
        retryOrder.loanType = "normal";
        retryOrder.side = "buy";
        retryOrder.quoteSize = "1.0000";
        ok = placeMarginOrder(retryOrder);
        if (ok) {
          float repayAmount = assetAfter.borrow + assetAfter.interest;
          ok = crossedMarginRepay(baseCoin, repayAmount);
        }
      } else {
        MarginOrder retryOrder;
        retryOrder.symbol = getInstId();
        retryOrder.orderType = "market";
        retryOrder.loanType = "autoRepay";
        retryOrder.side = "buy";
        retryOrder.quoteSize = safeFtos(residualUsdt * 1.02f, 4);
        ok = placeMarginOrder(retryOrder);
      }
      NOTICE("closeCrossedMarginPosition retry result ok=" << ok);
    }
  }

  return ok;
}

bool Bitget::flashRepay(const std::string& coin) {
  std::string requestPath = "/api/v2/margin/crossed/account/flash-repay";
  nlohmann::json j;
  j["coin"] = coin.empty() ? getBaseCoin() : coin;
  auto response = sendRequest(requestPath, "POST", "", j.dump());
  try {
    auto res = nlohmann::json::parse(response);
    std::string code = res["code"];
    if (code != API_SUCCESS) {
      ERROR("flashRepay API error: " << code << ", " << response);
      return false;
    }
    NOTICE("flashRepay ok coin=" << j["coin"].get<std::string>());
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("flashRepay error: " << e.what());
  } catch (const std::exception& e) {
    ERROR("flashRepay error: " << e.what());
  }
  return false;
}

bool Bitget::crossedMarginRepay(const std::string& coin, float amount) {
  nlohmann::json j;
  j["coin"] = coin;
  j["amount"] = safeFtos(amount, 8);

  NOTICE("crossedMarginRepay coin=" << coin << " amount=" << amount);
  auto response = sendRequest("/api/v2/margin/crossed/repay", "POST", "", j.dump());
  try {
    auto res = nlohmann::json::parse(response);
    std::string code = res["code"];
    if (code != API_SUCCESS) {
      ERROR("crossedMarginRepay error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("crossedMarginRepay parse error: " << e.what());
  } catch (const std::exception& e) {
    ERROR("crossedMarginRepay error: " << e.what());
  }
  return false;
}

bool Bitget::closeFuturesPosition() {
  Position position;
  if (!singlePosition(position)) {
    ERROR("closeFuturesPosition: failed to get position for " << getInstId());
    return false;
  }
  if (position.total == 0) {
    return true;
  }

  NOTICE("closeFuturesPosition snapshot symbol="
         << getInstId() << " holdSide=" << position.holdSide
         << " total=" << position.total << " available=" << position.available
         << " locked=" << position.locked);

  const float closeQty =
      (position.available > 0) ? position.available : position.total;
  const std::string size = adjustDecimalPlaces(closeQty, symbol_.volumePlace);
  std::string requestPath = "/api/v2/mix/order/place-order";

  auto submit_close = [&](const std::string& side, bool withTradeSide,
                          bool withHoldSide, bool withReduceOnly) {
    nlohmann::json j;
    j["symbol"] = getInstId();
    j["side"] = side;
    j["productType"] = "USDT-FUTURES";
    j["marginMode"] = "crossed";
    j["marginCoin"] = "USDT";
    j["orderType"] = "market";
    j["size"] = size;
    if (withTradeSide) {
      j["tradeSide"] = "close";
    }
    if (withHoldSide) {
      j["holdSide"] = position.holdSide;
    }
    if (withReduceOnly) {
      j["reduceOnly"] = "YES";
    }

    auto response = sendRequest(requestPath, "POST", "", j.dump());
    try {
      auto res = nlohmann::json::parse(response);
      std::string code = res["code"];
      if (code != API_SUCCESS) {
        ERROR("closePosition API error: code="
              << code << " side=" << side << " withTradeSide=" << withTradeSide
              << " withHoldSide=" << withHoldSide
              << " withReduceOnly=" << withReduceOnly << ", " << response);
        return false;
      }
      return true;
    } catch (const nlohmann::json::exception& e) {
      ERROR("closeFuturesPosition parse error: " << e.what()
                                                 << ", response:" << response);
    } catch (const std::exception& e) {
      ERROR("closeFuturesPosition error: " << e.what());
    }
    return false;
  };

  nlohmann::json jLongClose;
  jLongClose["symbol"] = getInstId();
  jLongClose["side"] = "sell";
  jLongClose["productType"] = "USDT-FUTURES";
  jLongClose["marginMode"] = "crossed";
  jLongClose["marginCoin"] = "USDT";
  jLongClose["orderType"] = "market";
  jLongClose["tradeSide"] = "close";
  jLongClose["holdSide"] = "long";
  jLongClose["size"] = size;
  auto respLongClose = sendRequest(requestPath, "POST", "", jLongClose.dump());
  try {
    auto res = nlohmann::json::parse(respLongClose);
    if (res["code"] == API_SUCCESS) return true;
  } catch (...) {
  }

  nlohmann::json jShortClose;
  jShortClose["symbol"] = getInstId();
  jShortClose["side"] = "buy";
  jShortClose["productType"] = "USDT-FUTURES";
  jShortClose["marginMode"] = "crossed";
  jShortClose["marginCoin"] = "USDT";
  jShortClose["orderType"] = "market";
  jShortClose["tradeSide"] = "close";
  jShortClose["holdSide"] = "short";
  jShortClose["size"] = size;
  auto respShortClose = sendRequest(requestPath, "POST", "", jShortClose.dump());
  try {
    auto res = nlohmann::json::parse(respShortClose);
    if (res["code"] == API_SUCCESS) return true;
  } catch (...) {
  }

  const std::string guessedCloseSide =
      (position.holdSide == "long") ? "sell" : "buy";
  const std::string oppositeCloseSide =
      (guessedCloseSide == "sell") ? "buy" : "sell";

  if (submit_close(guessedCloseSide, true, true, false)) return true;
  if (submit_close(guessedCloseSide, true, false, false)) return true;
  if (submit_close(guessedCloseSide, false, false, false)) return true;

  if (submit_close(oppositeCloseSide, true, true, false)) return true;
  if (submit_close(oppositeCloseSide, true, false, false)) return true;
  if (submit_close(oppositeCloseSide, false, false, false)) return true;

  if (submit_close("sell_single", false, false, true)) return true;
  if (submit_close("buy_single", false, false, true)) return true;
  if (submit_close("sell_single", false, false, false)) return true;
  if (submit_close("buy_single", false, false, false)) return true;
  return false;
}

bool Bitget::cancelOrder(const std::string& orderId) {
  if (orderId.empty()) {
    ERROR("Order ID is empty");
    return false;
  }

  std::string requestPath = "/api/v2/mix/order/cancel-order";
  std::string queryString;
  std::string queryBody;

  nlohmann::json j;

  j["symbol"] = getInstId();
  j["orderId"] = orderId;
  j["productType"] = "USDT-FUTURES";
  j["marginCoin"] = "USDT";

  queryBody = j.dump();

  auto response = sendRequest(requestPath, "POST", queryString, queryBody);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("cancelOrder error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::assets(float& available) {
  std::string requestPath = "/api/v2/mix/account/accounts";
  std::string queryString = "?productType=USDT-FUTURES";
  std::string response = sendRequestStatic(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    if (j["data"].size() != 1) return false;
    for (const auto& item : j["data"]) {
      available = safeStof(item["unionAvailable"]);
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("assets error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("exception: " << e.what());
  }
  return false;
}

bool Bitget::crossedMarginAsset(const std::string& coin,
                                CrossedMarginAsset& asset) {
  std::string requestPath = "/api/v2/margin/crossed/account/assets";
  std::string queryString = "?coin=" + coin;
  auto response = sendRequestStatic(requestPath, "GET", queryString);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    if (!j.contains("data") || !j["data"].is_array() || j["data"].empty()) {
      return false;
    }

    const auto& item = j["data"][0];
    asset.coin = item.value("coin", coin);
    asset.totalAmount = safeStof(item.value("totalAmount", "0"));
    asset.available = safeStof(item.value("available", "0"));
    asset.frozen = safeStof(item.value("frozen", "0"));
    asset.borrow = safeStof(item.value("borrow", "0"));
    asset.interest = safeStof(item.value("interest", "0"));
    asset.net = safeStof(item.value("net", "0"));
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("crossedMarginAsset error: " << e.what()
                                       << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
  return false;
}

bool Bitget::tickers(Ticker& ticker) {
  std::string requestPath = "/api/v2/mix/market/ticker";
  std::string queryString = "?productType=USDT-FUTURES&symbol=" + getInstId();
  auto response = sendRequest(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    if (j["data"].size() != 1) return false;
    for (const auto& item : j["data"]) {
      ticker.symbol = getInstId();
      ticker.lastPr = safeStof(item["lastPr"]);
      ticker.bidPr = safeStof(item["bidPr"]);
      ticker.askPr = safeStof(item["askPr"]);
    }

    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("tickers error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::tickers(std::map<std::string, FundingRate>& fundingRates) {
  std::string requestPath = "/api/v2/mix/market/tickers";
  std::string queryString = "?productType=USDT-FUTURES";
  auto response = sendRequest(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    fundingRates.clear();

    auto parse_item = [&fundingRates](const nlohmann::json& item) {
      if (!item.contains("symbol") || item["symbol"].is_null()) return;

      FundingRate fr;
      fr.symbol = item["symbol"];

      if (item.contains("fundingRate") && !item["fundingRate"].is_null()) {
        if (item["fundingRate"].is_string()) {
          fr.rate = safeStof(item["fundingRate"]);
        } else {
          fr.rate = item["fundingRate"].get<float>();
        }
      }

      if (std::fabs(fr.rate) < 0.005f) return;

      if (item.contains("lastPr") && !item["lastPr"].is_null()) {
        fr.lastPrice = item["lastPr"].is_string()
                           ? safeStof(item["lastPr"])
                           : item["lastPr"].get<float>();
      }

      if (item.contains("fundingTime") && !item["fundingTime"].is_null()) {
        if (item["fundingTime"].is_string()) {
          fr.nextFundingTime = safeStoll(item["fundingTime"]);
        } else {
          fr.nextFundingTime = item["fundingTime"].get<uint64_t>();
        }
      }

      fundingRates[fr.symbol] = fr;
    };

    if (j["data"].is_array()) {
      for (const auto& item : j["data"]) {
        parse_item(item);
      }
    } else if (j["data"].is_object()) {
      parse_item(j["data"]);
    }

    return !fundingRates.empty();
  } catch (const nlohmann::json::exception& e) {
    ERROR("tickers(all) error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::singlePosition(Position& position) {
  std::string requestPath = "/api/v2/mix/position/single-position";
  std::string queryString = "?symbol=" + getInstId();
  queryString += "&productType=USDT-FUTURE&marginCoin=USDT";

  auto response = sendRequest(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    nlohmann::json data = j["data"];
    if (j["data"].size() != 1) return false;
    for (const auto& item : j["data"]) {
      position.holdSide = item["holdSide"];
      position.available = safeStof(item["available"]);
      position.locked = safeStof(item["locked"]);
      position.total = safeStof(item["total"]);
      position.symbol = item["symbol"];
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("singlePosition error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

void Bitget::crossedMarginRemoveFromWhitelist(const std::string& symbol) {
  std::lock_guard<std::mutex> lock(crossedMarginSymbolsMtx_);
  crossedMarginSymbols_.erase(symbol);
}

bool Bitget::crossedMarginInterestRateAndLimit(const std::string& coin,
                                               MarginInterestInfo& info) {
  std::string requestPath = "/api/v2/margin/crossed/interest-rate-and-limit";
  std::string queryString = "?coin=" + coin;
  auto response = sendRequestStatic(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    if (j["code"].get<std::string>() != API_SUCCESS) {
      ERROR("crossedMarginInterestRateAndLimit API error: " << response);
      return false;
    }
    if (!j["data"].is_array() || j["data"].empty()) return false;
    const auto& item = j["data"][0];
    info.coin = coin;
    info.borrowable = item.value("borrowable", false);
    info.dailyInterestRate = safeStof(item.value("dailyInterestRate", "0"));
    info.maxBorrowableAmount = safeStof(item.value("maxBorrowableAmount", "0"));
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("crossedMarginInterestRateAndLimit error: " << e.what());
  } catch (const std::exception& e) {
    ERROR("crossedMarginInterestRateAndLimit error: " << e.what());
  }
  return false;
}

bool Bitget::allFuturesPositions(std::vector<Position>& positions) {
  std::string requestPath = "/api/v2/mix/position/all-position";
  std::string queryString = "?productType=USDT-FUTURES&marginCoin=USDT";
  auto response = sendRequestStatic(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("allFuturesPositions API error: " << code << ", " << response);
      return false;
    }
    positions.clear();
    if (!j["data"].is_array()) return true;
    for (const auto& item : j["data"]) {
      float total = safeStof(item.value("total", "0"));
      if (total <= 0) continue;
      Position p;
      p.symbol = item.value("symbol", "");
      p.holdSide = item.value("holdSide", "");
      p.available = safeStof(item.value("available", "0"));
      p.locked = safeStof(item.value("locked", "0"));
      p.total = total;
      positions.push_back(p);
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("allFuturesPositions error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("allFuturesPositions error: " << e.what());
  }
  return false;
}

bool Bitget::savingsAssets(Saving& saving) {
  std::string requestPath = "/api/v2/earn/savings/assets";
  std::string queryString = "?periodType=flexible";
  auto response = sendRequestStatic(requestPath, "GET", queryString);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }

    for (const auto& item : j["data"]["resultList"]) {
      saving.productCoin = item["productCoin"];
      if (saving.productCoin != "USDT") continue;
      saving.productId = item["productId"];
      saving.orderId = item["orderId"];
      saving.holdAmount = item["holdAmount"];
      saving.periodType = item["periodType"];
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("symbols error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::savingsSubscribe(Saving& saving) {
  std::string requestPath = "/api/v2/earn/savings/subscribe";
  std::string queryString;
  std::string queryBody;

  nlohmann::json j;

  j["productId"] = saving.productId;
  j["periodType"] = saving.periodType;
  j["amount"] = saving.holdAmount;

  queryBody = j.dump();

  auto response =
      sendRequestStatic(requestPath, "POST", queryString, queryBody);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("savingsSubscribe error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::savingsRedeem(Saving& saving) {
  std::string requestPath = "/api/v2/earn/savings/redeem";
  std::string queryString;
  std::string queryBody;

  nlohmann::json j;

  j["productId"] = saving.productId;
  j["periodType"] = saving.periodType;
  j["amount"] = saving.holdAmount;

  queryBody = j.dump();

  auto response =
      sendRequestStatic(requestPath, "POST", queryString, queryBody);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("savingsRedeem error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::transfer(const std::string& amount, const std::string& src,
                      const std::string& dst) {
  std::string requestPath = "/api/v2/spot/wallet/transfer";
  std::string queryString;
  std::string queryBody;

  nlohmann::json j;

  j["fromType"] = src;
  j["toType"] = dst;
  j["amount"] = amount;
  j["coin"] = "USDT";

  queryBody = j.dump();

  auto response =
      sendRequestStatic(requestPath, "POST", queryString, queryBody);

  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    std::string msg = j["msg"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("transfer error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }

  return false;
}

bool Bitget::fundingRate(FundingRate& fr) {
  std::string requestPath = "/api/v2/mix/market/current-fund-rate";
  std::string queryString =
      "?symbol=" + getInstId() + "&productType=usdt-futures";
  auto response = sendRequest(requestPath, "GET", queryString);
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    nlohmann::json data = j["data"];
    if (data.is_array()) {
      if (data.empty()) return false;
      data = data[0];
    }

    fr.symbol = getInstId();
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
    ERROR("fundingRate error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
  return false;
}

bool Bitget::crossedMarginSymbolSupported(const std::string& symbol) {
  {
    std::lock_guard<std::mutex> lock(crossedMarginSymbolsMtx_);
    if (crossedMarginSymbolsLoaded_) {
      return crossedMarginSymbols_.count(symbol) > 0;
    }
  }

  const std::string requestPath = "/api/v2/margin/currencies";
  const std::string response = sendRequestStatic(requestPath, "GET");

  std::lock_guard<std::mutex> lock(crossedMarginSymbolsMtx_);
  if (!crossedMarginSymbolsLoaded_) {
    try {
      auto j = nlohmann::json::parse(response);
      if (j["code"].get<std::string>() == API_SUCCESS && j["data"].is_array()) {
        for (const auto& item : j["data"]) {
          if (!item.contains("symbol") || item["symbol"].is_null()) continue;
          bool borrowable = item.contains("isCrossBorrowable") &&
                            item["isCrossBorrowable"].is_boolean() &&
                            item["isCrossBorrowable"].get<bool>();
          if (borrowable) {
            crossedMarginSymbols_.insert(item["symbol"].get<std::string>());
          }
        }
        crossedMarginSymbolsLoaded_ = true;
        NOTICE("Loaded " << crossedMarginSymbols_.size()
                         << " cross margin symbols");
      } else {
        ERROR("crossedMarginSymbolSupported API error: " << response);
      }
    } catch (const nlohmann::json::exception& e) {
      ERROR("crossedMarginSymbolSupported parse error: " << e.what());
    } catch (const std::exception& e) {
      ERROR("crossedMarginSymbolSupported error: " << e.what());
    }
  }
  return crossedMarginSymbols_.count(symbol) > 0;
}
