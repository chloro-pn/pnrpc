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

#define SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(integral_type) \
template<> \
struct RpcCreator<integral_type> { \
  static integral_type create(const char* ptr, size_t len) { \
    return integralParse<integral_type>(ptr, len); \
  } \
\
  static void to_raw_bytes(const integral_type& request, std::string& appender) { \
    integralSeri(request, appender); \
  } \
};

SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(uint8_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(int8_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(uint16_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(int16_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(uint32_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(int32_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(uint64_t)
SPECIALIZATION_INTEGRAL_TYPE_FOR_RPC_CREATOR(int64_t)

}