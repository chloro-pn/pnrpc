#pragma once

#include <string>
#include <cstring>
#include <concepts>

#include "pnrpc/rpc_concept.h"
#include "pnrpc/util.h"

namespace pnrpc {

template<typename RType> requires RpcTypeConcept<RType>
struct RpcCreator {
  static RType create(const char* ptr, size_t len) {
    return RType::create_from_raw_bytes(ptr, len);
  }

  static void to_raw_bytes(const RType& request, std::string& appender) {
    RType::to_raw_bytes(request, appender);
  }
};

template<>
struct RpcCreator<std::string> {
  static std::string create(const char* ptr, size_t len) {
    return std::string(ptr, len);
  }

  static void to_raw_bytes(const std::string& request, std::string& appender) {
    appender.append(request);
  }
};

template<>
struct RpcCreator<uint32_t> {
  static uint32_t create(const char* ptr, size_t len) {
    return integralParse<uint32_t>(ptr, len);
  }

  static void to_raw_bytes(const uint32_t& request, std::string& appender) {
    integralSeri(request, appender);
  }
};

}