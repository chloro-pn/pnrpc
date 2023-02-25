#pragma once

#include "pnrpc/rpc_server.h"
#include <string>
#include <memory>

struct RPC_add_request_type {
  std::string str;

  static std::unique_ptr<RPC_add_request_type> create_from_raw_bytes(const char* ptr, size_t len) {
    std::unique_ptr<RPC_add_request_type> ret(new RPC_add_request_type);
    ret->str = std::string(ptr, len);
    return ret;
  }

  static void to_raw_bytes(const RPC_add_request_type& request, std::string& appender) {
    appender.append(request.str);
  }
};

RPC_DECLARE(add, RPC_add_request_type, std::string, 0x00)

RPC_DECLARE(num_add, uint32_t, uint32_t, 0x01)