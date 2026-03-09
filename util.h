#ifndef _UTIL_H
#define _UTIL_H

#include <cstdint>
#include <string>
#include <vector>

#include "Poco/LocalDateTime.h"
#include "Poco/Thread.h"
#include "data.h"

#define THIS_YEAR Poco::LocalDateTime().year()
#define THIS_MONTH Poco::LocalDateTime().month()
#define THIS_DAY Poco::LocalDateTime().day()
#define THIS_HOUR Poco::LocalDateTime().hour()
#define THIS_MINUTE Poco::LocalDateTime().minute()
#define THIS_SECOND Poco::LocalDateTime().second()
#define THIS_MILLISEC Poco::LocalDateTime().millisecond()
#define SLEEP_MS(ms) Poco::Thread::sleep(ms)

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const T& enumValue, const int intValue) {
  return static_cast<int>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const T& enumValue, const uint32_t intValue) {
  return static_cast<uint32_t>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type operator==(
    const uint32_t intValue, const T& enumValue) {
  return static_cast<uint32_t>(enumValue) == intValue;
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, std::ostream&>::type operator<<(
    std::ostream& os, T value) {
  return os << static_cast<typename std::underlying_type<T>::type>(value);
}

std::string convertTimestamp(const std::string& ts);

std::string convertISO8601Timestamp(const std::string& ts);

std::string getTimestamp();

std::string getISO8601Timestamp();

std::string getTimestampMs();

uint64_t getCurrentTimeMs();

uint64_t safeStoll(const std::string& str);

int safeStoi(const std::string& str);

float safeStof(const std::string& str);

std::string safeFtos(float value, int places);

bool areFloatsEqual(float a, float b, float epsilon = 1e-6);

std::string adjustDecimalPlaces(float num, const std::string& epsilon);

std::string convertRemark(const std::string& remark);

std::string hmac_sha256(const std::string &key, const std::string &data);

std::string base64_encode(const std::string &input);

float adjustToNearestMultiple(float a, float b);

void sendMessage(const std::string& message, bool force = false);
#endif