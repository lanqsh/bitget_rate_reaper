#include "util.h"

#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
#include "tracer.h"

namespace {
std::map<char, std::string> kEscapingMap = {
    {' ', "%20"}, {'"', "%22"}, {'#', "%23"},  {'%', "%25"}, {'&', "%26"},
    {'(', "%28"}, {')', "%29"}, {'+', "%2B"},  {',', "%2C"}, {'/', "%2F"},
    {':', "%3A"}, {';', "%3B"}, {'<', "%3C"},  {'=', "%3D"}, {'>', "%3E"},
    {'?', "%3F"}, {'@', "%40"}, {'\\', "%5C"}, {'|', "%7C"}, {'`', "\\`"},
    {'*', "\\*"}, {'$', "\\$"}, {'[', "%5B"},  {']', "%5D"}, {'^', "%5E"},
    {'{', "%7B"}, {'}', "%7D"}, {'~', "%7E"}};
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int len;

  HMAC(EVP_sha256(), key.c_str(), key.length(), (unsigned char*)data.c_str(),
       data.length(), digest, &len);

  return std::string((char*)digest, len);
}

std::string base64_encode(const std::string& input) {
  BIO *bio, *b64;
  char* bufferPtr;
  long length;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, input.c_str(), input.length());
  BIO_flush(bio);

  length = BIO_get_mem_data(bio, &bufferPtr);
  std::string result(bufferPtr, length);

  BIO_free_all(bio);
  return result;
}

std::string convertTimestamp(const std::string& ts) {
  if (ts.empty()) return "";
  std::time_t timestamp = std::stoull(ts) / 1000;
  auto now_ms = timestamp % 1000;
  std::tm* timeinfo = std::localtime(&timestamp);

  std::stringstream ss;
  ss << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::string convertISO8601Timestamp(const std::string& ts) {
  if (ts.empty()) return "";
  std::time_t timestamp = std::stoull(ts) / 1000;
  auto now_ms = timestamp % 1000;
  std::tm* timeinfo = std::localtime(&timestamp);

  std::stringstream ss;
  ss << std::put_time(timeinfo, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3)
     << std::setfill('0') << now_ms << 'Z';
  return ss.str();
}

std::string getTimestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_seconds = system_clock::to_time_t(now);
  std::tm tm = {};

  localtime_r(&now_seconds, &tm);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

std::string getISO8601Timestamp() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto now_seconds = system_clock::to_time_t(now);
  std::tm tm = {};

  gmtime_r(&now_seconds, &tm);
  auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3)
     << std::setfill('0') << now_ms.count() << 'Z';
  return ss.str();
}

std::string getTimestampMs() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  return std::to_string(ms);
}

uint64_t getCurrentTimeMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

uint64_t safeStoll(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoull(str);
  } catch (...) {
    ERROR("safeStoi error: " << str);
    return 0;
  }
}

int safeStoi(const std::string& str) {
  if (str.empty()) return 0;

  try {
    return std::stoi(str);
  } catch (...) {
    ERROR("safeStoi error: " << str);
    return 0;
  }
}

float safeStof(const std::string& str) {
  if (str.empty()) return 0.0f;

  try {
    return std::stof(str);
  } catch (...) {
    ERROR("safeStof error: " << str);
    return 0.0f;
  }
}

std::string safeFtos(float value, int places) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(places);
  oss << value;
  return oss.str();
}

bool areFloatsEqual(float a, float b, float epsilon) {
  return std::fabs(a - b) < epsilon;
}

std::string adjustDecimalPlaces(float num, const std::string& epsilon) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(safeStoi(epsilon)) << num;

  return oss.str();
}

std::string convertRemark(const std::string& remark) {
  std::string res;
  for (int i = 0; i < remark.size(); ++i) {
    char c = remark.at(i);
    auto it = kEscapingMap.find(c);
    if (it != kEscapingMap.end()) {
      res += kEscapingMap[c];
    } else {
      res += remark.at(i);
    }
  }
  return res;
}

float adjustToNearestMultiple(float a, float b) {
  if (b == 0.0f) {
    return a;
  }

  float quotient = a / b;
  int n = static_cast<int>(std::round(quotient));
  return n * b;
}

void sendMessage(const std::string& message, bool force) {
  std::string endpoint = "curl -s " + kConfig.barkServer;
  std::string ring = "?level=critical&volume=1";

  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm* local_time = std::localtime(&now_time);
  int current_hour = local_time->tm_hour;
  if (!force && current_hour < 8) {
    ring = "";
  }

  std::string cmd = convertRemark(message);
  cmd = endpoint + cmd + ring + " > /dev/null";
  system(cmd.c_str());
}