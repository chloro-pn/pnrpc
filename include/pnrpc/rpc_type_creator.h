#pragma once

#include <memory>
#include <string>
#include <cstring>

#include "pnrpc/rpc_concept.h"

namespace pnrpc {

template<typename RType> requires RpcTypeConcept<RType>
struct RpcCreator {
  static std::unique_ptr<RType> create(const char* ptr, size_t len) {
    return RType::create_from_raw_bytes(ptr, len);
  }

  static void to_raw_bytes(const RType& request, std::string& appender) {
    RType::to_raw_bytes(request, appender);
  }
};

template<>
struct RpcCreator<std::string> {
  static std::unique_ptr<std::string> create(const char* ptr, size_t len) {
    return std::make_unique<std::string>(ptr, len);
  }

  static void to_raw_bytes(const std::string& request, std::string& appender) {
    appender.append(request);
  }
};

template<>
struct RpcCreator<uint32_t> {
  static std::unique_ptr<uint32_t> create(const char* ptr, size_t len) {
    uint32_t tmp;
    memcpy(&tmp, ptr, len);
    return std::make_unique<uint32_t>(tmp);
  }

  static void to_raw_bytes(const uint32_t& request, std::string& appender) {
    appender.append((const char*)&request, sizeof(request));
  }
};

}