#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "bridge/object.h"
#include "pnrpc/log.h"
#include "pnrpc/rpc_ret_code.h"
#include "pnrpc/rpc_type_creator.h"
#include "pnrpc/util.h"

namespace pnrpc {

template <typename RpcType>
class RequestPackager {
 public:
  void seri_request_package(const RpcType& package, std::string& appender, uint32_t pcode, bool eof) {
    pcodeSeri(pcode, appender);
    eofSeri(eof, appender);
    RpcCreator<RpcType>::to_raw_bytes(package, appender);
  }

  RpcType parse_request_package(const std::string& msg, uint32_t& pcode, bool& eof) {
    const char* ptr = &msg[0];
    size_t buf_len = msg.size();
    pcode = ParsePcode(ptr, buf_len);
    eof = ParseEofFlag(ptr, buf_len);
    auto pkg = RpcCreator<RpcType>::create(ptr, buf_len);
    return pkg;
  }
};

// 这个特化用来在不知道请求参数类型的时候解析基本格式
template <>
class RequestPackager<void> {
 public:
  std::string_view parse_request_package(const std::string& msg, uint32_t& pcode, bool& eof) {
    const char* ptr = &msg[0];
    size_t buf_len = msg.size();
    pcode = ParsePcode(ptr, buf_len);
    eof = ParseEofFlag(ptr, buf_len);
    std::string_view request_view(ptr, buf_len);
    return request_view;
  }
};

template <typename RpcType>
class ResponsePackager {
 public:
  void seri_response_package(const RpcType& package, std::string& appender, uint32_t ret_code, bool eof) {
    bridge::BridgePool bp;
    auto root = bp.map();
    root->Insert("ret_code", bp.data(ret_code));
    root->Insert("eof", bp.data(eof == true ? uint32_t(0) : uint32_t(1)));
    std::string pkg_binary;
    RpcCreator<RpcType>::to_raw_bytes(package, pkg_binary);
    root->Insert("response", bp.data(std::move(pkg_binary)));
    auto response = bridge::Serialize(std::move(root), bp);
    appender.append(response);
  }

  struct ResponseInfo {
    RpcType response;
    uint32_t ret_code;
    std::string err_msg;
    bool eof;
  };

  ResponseInfo parse_response_package(const std::string& msg) {
    ResponseInfo ri;
    bridge::BridgePool bp;
    auto root = bridge::Parse(msg, bp);
    bridge::ObjectWrapper ow(root.get());
    ri.ret_code = ow["ret_code"].Get<uint32_t>().value();
    auto response = ow["response"].Get<std::string>().value();
    if (ri.ret_code != RPC_OK) {
      ri.err_msg = response;
    } else {
      ri.response = RpcCreator<RpcType>::create(&response[0], response.size());
      ri.eof = ow["eof"].Get<uint32_t>().value() == 0;
    }
    return ri;
  }
};

// 这个特化用来回复错误信息
template <>
class ResponsePackager<void> {
 public:
  void seri_error_package(const std::string& err_msg, uint32_t ret_code, std::string& appender) {
    assert(ret_code != RPC_OK);
    bridge::BridgePool bp;
    auto root = bp.map();
    root->Insert("ret_code", bp.data(ret_code));
    root->Insert("response", bp.data(err_msg));
    auto tmp = bridge::Serialize(std::move(root), bp);
    appender.append(tmp);
  }
};

}  // namespace pnrpc