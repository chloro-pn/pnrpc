#pragma once

#include <memory>
#include <string>
#include <cstring>

template<typename RequestType>
struct RequestCreator {
  static std::unique_ptr<RequestType> create(const char* ptr, size_t len) {
    return RequestType::create_from_raw_bytes(ptr, len);
  }

  static void to_raw_bytes(const RequestType& request, std::string& appender) {
    return RequestType::to_raw_bytes(request, appender);
  }
};

template<>
struct RequestCreator<std::string> {
  static std::unique_ptr<std::string> create(const char* ptr, size_t len) {
    return std::make_unique<std::string>(ptr, len);
  }

  static void to_raw_bytes(const std::string& request, std::string& appender) {
    appender.append(request);
  }
};

template<>
struct RequestCreator<uint32_t> {
  static std::unique_ptr<uint32_t> create(const char* ptr, size_t len) {
    uint32_t tmp;
    memcpy(&tmp, ptr, len);
    return std::make_unique<uint32_t>(tmp);
  }

  static void to_raw_bytes(const uint32_t& request, std::string& appender) {
    appender.append((const char*)&request, sizeof(request));
  }
};