#pragma once

#include "pnrpc/rpc_server.h"
#include "bridge/object.h"

#include <string>
#include <memory>

struct Desc_request_type {
  std::string str;
  uint32_t age;
  uint32_t man;

  static std::unique_ptr<Desc_request_type> create_from_raw_bytes(const char* ptr, size_t len) {
    std::unique_ptr<Desc_request_type> ret(new Desc_request_type);
    bridge::BridgePool bp;
    std::string tmp(ptr, len);
    auto root = bridge::Parse(tmp, bp);
    bridge::ObjectWrapper wrapper(root.get());
    ret->str = wrapper["str"].Get<std::string>().value();
    ret->age = wrapper["age"].Get<uint32_t>().value();
    ret->man = wrapper["man"].Get<uint32_t>().value();
    return ret;
  }

  static void to_raw_bytes(const Desc_request_type& request, std::string& appender) {
    bridge::BridgePool bp;
    auto root = bp.map();
    root->Insert("str", bp.data(request.str));
    root->Insert("age", bp.data(request.age));
    root->Insert("man", bp.data(request.man));
    appender.append(bridge::Serialize(std::move(root), bp));
  }
};


RPC_DECLARE(Echo, std::string, std::string, 0x00)

RPC_DECLARE(Sleep, uint32_t, uint32_t, 0x01)

// 可以给rpc方法绑定自定义的io_context
RPC_DECLARE_BIND(Desc, Desc_request_type, std::string, 0x02)

RPC_DECLARE(async_task, std::string, std::string, 0x03)