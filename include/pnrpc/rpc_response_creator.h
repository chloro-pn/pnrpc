#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <memory>

#include "pnrpc/util.h"

namespace pnrpc {

template <typename ResponseType>
struct ResponseCreator {
  static std::string to_raw_bytes(const ResponseType& rt) {
    return ResponseType::to_raw_bytes(rt);
  }

  static std::unique_ptr<ResponseType> create_from_raw_bytes(const char* ptr, size_t len) {
    return ResponseType::create_from_raw_bytes(ptr, len);
  }
};

template <>
struct ResponseCreator<std::string> {
  static std::string to_raw_bytes(const std::string& rt) {
    return rt;
  }

  static std::unique_ptr<std::string> create_from_raw_bytes(const char* ptr, size_t len) {
    return std::make_unique<std::string>(ptr, len);
  }
};

template <>
struct ResponseCreator<uint32_t> {
  static std::string to_raw_bytes(const uint32_t& rt) {
    std::string ret;
    integralSeri(rt, ret);
    return ret;
  }

  static std::unique_ptr<uint32_t> create_from_raw_bytes(const char* ptr, size_t len) {
    uint32_t tmp = integralParse<uint32_t>(ptr, len);
    return std::make_unique<uint32_t>(tmp);
  }
};

}