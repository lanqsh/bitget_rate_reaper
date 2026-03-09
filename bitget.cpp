#include "bitget.h"

#include <cmath>
#include <chrono>
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

Bitget::~Bitget() {}

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
  CURLcode res;
  std::string readBuffer;
  CURL* curl = curl_easy_init();
  if (curl) {
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

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }
    if (method == "DELETE") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      ERROR_("api", "curl_easy_perform() failed: " << curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
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
  CURLcode res;
  std::string readBuffer;
  CURL* curl = curl_easy_init();
  if (curl) {
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

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (method == "POST") {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }
    if (method == "DELETE") {
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      ERROR_("api", "curl_easy_perform() failed: " << curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
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
  // j["presetStopSurplusPrice"] =
  //     adjustDecimalPlaces(order.tp_price, symbol_.pricePlace);
  // j["presetStopSurplusExecutePrice"] =
  //     adjustDecimalPlaces(order.tp_price, symbol_.pricePlace);

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

bool Bitget::closePosition() {
  // Get current position to determine size and direction
  Position position;
  if (!singlePosition(position)) {
    ERROR("closePosition: failed to get position for " << getInstId());
    return false;
  }
  if (position.total == 0) {
    return true;  // nothing to close
  }

  std::string requestPath = "/api/v2/mix/order/place-order";
  nlohmann::json j;
  j["symbol"] = getInstId();
  // close: opposite side of the open position
  j["side"] = (position.holdSide == "long") ? "sell" : "buy";
  j["productType"] = "USDT-FUTURES";
  j["marginMode"] = "crossed";
  j["marginCoin"] = "USDT";
  j["tradeSide"] = "close";
  j["orderType"] = "market";
  j["size"] = adjustDecimalPlaces(position.total, symbol_.volumePlace);

  auto response = sendRequest(requestPath, "POST", "", j.dump());
  try {
    auto j = nlohmann::json::parse(response);
    std::string code = j["code"];
    if (code != API_SUCCESS) {
      ERROR("API error: " << code << ", " << response);
      return false;
    }
    return true;
  } catch (const nlohmann::json::exception& e) {
    ERROR("closePosition error: " << e.what() << ", response:" << response);
  } catch (const std::exception& e) {
    ERROR("error: " << e.what());
  }
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
  std::string queryString = "?symbol=" + getInstId() + "&productType=usdt-futures";
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

