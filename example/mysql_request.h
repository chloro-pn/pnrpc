#pragma once

#include "bridge/object.h"
#include "pnrpc/rpc_declare.h"

struct MysqlRequestRpcT {
  static MysqlRequestRpcT create_from_raw_bytes(const char* ptr, size_t len) {
    MysqlRequestRpcT request;
    bridge::BridgePool bp;
    auto root = bridge::Parse(std::string(ptr, len), bp);
    bridge::ObjectWrapper wrapper(root.get());
    request.user_name = wrapper["user_name"].Get<std::string>().value();
    request.password = wrapper["password"].Get<std::string>().value();
    request.db_name = wrapper["db_name"].Get<std::string>().value();
    return request;
  }

  static void to_raw_bytes(const MysqlRequestRpcT& request, std::string& appender) {
    bridge::BridgePool bp;
    auto root = bp.map();
    root->Insert("user_name", bp.data(request.user_name));
    root->Insert("password", bp.data(request.password));
    root->Insert("db_name", bp.data(request.db_name));
    appender.append(bridge::Serialize(std::move(root), bp));
  }

  std::string user_name;
  std::string password;
  std::string db_name;
};

RPC_DECLARE(MysqlRequest, MysqlRequestRpcT, std::string, 0x06, pnrpc::RpcType::Simple, OVERRIDE_PROCESS)