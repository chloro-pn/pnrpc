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
#include "pnrpc/util.h"

class RpcProcessorBase {
 public:
  RpcProcessorBase(size_t pcode) : code(pcode) {

  }

  virtual void create_request_from_raw_bytes(const char* ptr, size_t len) = 0;
  virtual int process() = 0;
  virtual std::string create_response_to_raw_btes() = 0;

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

  std::string HandleRequest(const char* ptr, size_t len) {
    if (len <= sizeof(uint32_t)) {
      std::cerr << "invalid net packet." << std::endl;
      return "";
    }
    uint32_t pcode = ParsePcode(ptr, len);
    auto processor = GetProcessor(pcode);
    if (processor == nullptr) {
      std::cerr << "invalid rpc request : " << pcode << std::endl;
      return "";
    }
    processor->create_request_from_raw_bytes(ptr, len - sizeof(uint32_t));
    processor->process();
    std::string ret = processor->create_response_to_raw_btes();
    return ret;
  }

  std::unique_ptr<RpcProcessorBase> GetProcessor(size_t pcode) {
    auto it = funcs_.find(pcode);
    if (it == funcs_.end()) {
      return nullptr;
    }
    return it->second(pcode);
  }

  uint32_t ParsePcode(const char*& ptr, size_t len) const {
    size_t pcode = integralParse<uint32_t>(ptr, len);
    ptr += sizeof(uint32_t);
    return static_cast<uint32_t>(pcode);
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

  std::string create_response_to_raw_btes() override {
    std::string ret = ResponseCreator<response_t>::to_raw_bytes(*response);
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
    uint32_t pcode = static_cast<uint32_t>(code);
    integralSeri(pcode, ret);
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