#pragma once

#include "pnrpc/net_server.h"
#include "bridge/object.h"

#include <vector>

struct RpcSumRequestT {
  static std::unique_ptr<RpcSumRequestT> create_from_raw_bytes(const char* ptr, size_t len) {
    auto request = std::make_unique<RpcSumRequestT>();
    bridge::BridgePool bp;
    auto root = bridge::Parse(std::string(ptr, len), bp);
    bridge::ObjectWrapper wrapper(root.get());
    for(size_t i = 0; i < wrapper.Size(); ++i) {
      uint32_t num = wrapper[i].Get<uint32_t>().value();
      request->nums.push_back(num);
    }
    return request;
  }

  static void to_raw_bytes(const RpcSumRequestT& request, std::string& appender) {
    bridge::BridgePool bp;
    auto root = bp.array();
    for(size_t i = 0; i < request.nums.size(); ++i) {
      root->Insert(bp.data(request.nums[i]));
    }
    appender.append(bridge::Serialize(std::move(root), bp));
  }

  std::vector<uint32_t> nums;
};

RPC_DECLARE(Sum, RpcSumRequestT, uint32_t, 0x00, OVERRIDE_PROCESS)