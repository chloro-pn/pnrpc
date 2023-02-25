#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <iostream>

#include "pnrpc/rpc_request_creator.h"
#include "pnrpc/rpc_response_creator.h"

class RpcProcessorBase {
 public:
  RpcProcessorBase(size_t pcode) : code(pcode) {

  }

  virtual void create_request_from_raw_bytes(const char* ptr, size_t len) = 0;
  virtual int process() = 0;
  virtual std::vector<char> create_response_to_raw_btes() = 0;

  size_t get_pcode() const { return code; }

 private:
  size_t code;
};

class RpcServer {
 public:
  using CreatorFunction = std::function<std::unique_ptr<RpcProcessorBase>(size_t)>;

  static RpcServer& Instance() {
    static RpcServer obj;
    return obj;
  }

  void RegisterRpc(size_t pcode, CreatorFunction cf) {
    if (funcs_.count(pcode) != 0) {
      std::cerr << "warnning : duplicate rpc code " << pcode << std::endl;
    }
    funcs_[pcode] = std::move(cf);
  }

  std::unique_ptr<RpcProcessorBase> GetProcessor(size_t pcode) {
    auto it = funcs_.find(pcode);
    if (it == funcs_.end()) {
      return nullptr;
    }
    return it->second(pcode);
  }

 private:
  std::unordered_map<size_t, CreatorFunction> funcs_;

  RpcServer() {}
};

template <typename RequestType, typename ResponseType>
class RpcProcessor : public RpcProcessorBase {
  using request_t = RequestType;
  using response_t = ResponseType;

 public:
  explicit RpcProcessor(size_t pcode) : request(nullptr), response(nullptr), RpcProcessorBase(pcode) {}

  void create_request_from_raw_bytes(const char* ptr, size_t len) override {
    request = RequestCreator<request_t>::create(ptr, len);
  }

  virtual int process() = 0;

  std::vector<char> create_response_to_raw_btes() override {
    std::vector<char> ret = ResponseCreator<response_t>::to_raw_bytes(*response);
    return ret;
  }

 protected:
  std::unique_ptr<request_t> request;
  std::unique_ptr<response_t> response;
};

template <typename RequestType, typename ResponseType>
class RpcStub {
  using request_t = RequestType;
  using response_t = ResponseType;

 public:
  explicit RpcStub (size_t pcode) : request(nullptr), response(nullptr), code(pcode) {}

  void set_request(std::unique_ptr<request_t>&& r) {
    request = std::move(r);
  }

  std::string create_request_message() {
    std::string ret;
    ret.resize(sizeof(uint32_t));
    // todo 考虑网络大小端问题
    *reinterpret_cast<uint32_t*>(&ret[0]) = static_cast<uint32_t>(code);
    RequestCreator<request_t>::to_raw_bytes(*request, ret);
    return ret;
  }

  const response_t& create_response(const char* ptr, size_t len) {
    response = ResponseCreator<response_t>::create_from_raw_bytes(ptr, len);
    return *response;
  }

 private:
  std::unique_ptr<request_t> request;
  std::unique_ptr<response_t> response;
  size_t code;
};

#define RPC_DECLARE(funcname, request_t, response_t, pcode) \
class RPC ## funcname : public RpcProcessor<request_t, response_t> { \
 public: \
  RPC ## funcname() : RpcProcessor<request_t, response_t>(pcode) {} \
  int process() override; \
}; \
\
class RPC ## funcname ## STUB : public RpcStub<request_t, response_t> { \
 public: \
  RPC ## funcname ## STUB () : RpcStub<request_t, response_t>(pcode) {} \
}; \

#define REGISTER_RPC(funcname, pcode) \
  RpcServer::Instance().RegisterRpc(pcode, [](size_t) -> std::unique_ptr<RpcProcessorBase> { \
    return std::make_unique<RPC ## funcname>(); \
  });