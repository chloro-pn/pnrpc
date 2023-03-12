#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <string>

#include "pnrpc/rpc_concept.h"
#include "pnrpc/rpc_ret_code.h"
#include "pnrpc/util.h"
#include "pnrpc/log.h"
#include "pnrpc/rebind_ctx.h"
#include "pnrpc/rpc_type_creator.h"
#include "bridge/object.h"
#include "asio.hpp"

#include "asio/dispatch.hpp"

namespace pnrpc {

// 由使用者保证，在RpcProcessorBase类型的对象生命周期内，running_io_总是有效的。
class RpcProcessorBase {
 public:
  RpcProcessorBase(size_t pcode) : code(pcode), running_io_(nullptr) {

  }

  virtual void create_request_from_raw_bytes(const char* ptr, size_t len) = 0;
  virtual asio::awaitable<void> process() = 0;
  virtual std::string create_response_to_raw_btes() = 0;

  void set_io_context(asio::io_context& io) {
    running_io_ = &io;
  }

  asio::io_context& get_io_context() {
    return *running_io_;
  }

  size_t get_pcode() const { return code; }

  // 用户可以通过重写此方法将本rpc分配给自定义的handle_io处理
  virtual asio::io_context* bind_io_context() {
    return nullptr;
  }

  // 用户可以通过重写此方法实现限流算法。注意：
  // 每个RpcProcessorBase对象只负责处理一次rpc请求，因此需要将限流信息存储在生命周期更长的对象中而不是RpcProcessorBase对象中。
  virtual bool restrictor() {
    return true;
  }

 private:
  size_t code;
  asio::io_context* running_io_;
};

class RpcServer {
 public:
  using CreatorFunction = std::function<std::unique_ptr<RpcProcessorBase>(size_t)>;

  struct HandleInfo {
    std::string response;
    int ret_code;
    uint32_t pcode;
    double process_ms;
    asio::io_context* bind_ctx;
    std::unique_ptr<asio::ip::tcp::socket> socket;
  };

  static RpcServer& Instance() {
    static RpcServer obj;
    return obj;
  }

  void RegisterRpc(size_t pcode, CreatorFunction cf) {
    if (funcs_.count(pcode) != 0) {
      PNRPC_LOG_WARN("duplicate rpc code : {}", pcode);
    }
    funcs_[pcode] = std::move(cf);
  }

  asio::awaitable<HandleInfo> HandleRequest(const char* ptr, size_t len, asio::io_context& io, asio::ip::tcp::socket socket) {
    HandleInfo handle_info;
    handle_info.ret_code = RPC_OK;
    handle_info.pcode = 0;
    handle_info.bind_ctx = nullptr;
    std::string response;
    if (len <= sizeof(uint32_t)) {
      handle_info.ret_code = RPC_NET_ERR;
      response = "invalid net packet";
      PNRPC_LOG_ERROR("invalid net packet");
    } else {
      handle_info.pcode = ParsePcode(ptr, len);
      auto processor = GetProcessor(handle_info.pcode);
      if (processor == nullptr) {
        handle_info.ret_code = RPC_INVALID_PCODE;
        response = "not found rpc request, pcode == " + std::to_string(handle_info.pcode);
      } else {
        // 在调用bind_io_context之前解析请求，这意味着可以根据请求信息做更细粒度的分流。
        processor->create_request_from_raw_bytes(ptr, len - sizeof(uint32_t));
        handle_info.bind_ctx = &io;
        processor->set_io_context(io);
        auto bind_ctx = processor->bind_io_context();
        // 如果用户给该rpc绑定了io_context，则将本协程调度给该io_context执行，注意，需要将socket绑定的ctx和processor注册的ctx同步修改
        if (bind_ctx != nullptr) {
          handle_info.bind_ctx = bind_ctx;
          processor->set_io_context(*bind_ctx);
          socket = rebind_ctx(std::move(socket), *bind_ctx);
          co_await asio::dispatch(asio::bind_executor(*bind_ctx, asio::use_awaitable));
        }
        // 在调度到执行本rpc的io_context上之后进行限流判定，这意味着可以通过请求信息、io_context信息等做更细粒度的限流
        bool overload = !processor->restrictor();
        if (overload) {
          handle_info.ret_code = RPC_OVERFLOW;
          response = "rpc request overflow";
        } else {
          Timer timer;
          timer.Start();
          co_await processor->process();
          handle_info.process_ms = timer.End();
          handle_info.ret_code = RPC_OK;
          response = processor->create_response_to_raw_btes();
        }
      }
    }
    handle_info.socket = std::make_unique<asio::ip::tcp::socket>(std::move(socket));
    bridge::BridgePool bp;
    auto root = bp.map();
    root->Insert("ret_code", bp.data(static_cast<int32_t>(handle_info.ret_code)));
    root->Insert("response", bp.data(std::move(response)));
    handle_info.response = bridge::Serialize(std::move(root), bp);
    co_return handle_info;
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

template <typename RequestType, typename ResponseType> requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcProcessor : public RpcProcessorBase {
  using request_t = RequestType;
  using response_t = ResponseType;

 public:
  explicit RpcProcessor(size_t pcode) : request(nullptr), response(nullptr), RpcProcessorBase(pcode) {}

  void create_request_from_raw_bytes(const char* ptr, size_t len) override {
    request = RpcCreator<request_t>::create(ptr, len);
  }

  virtual asio::awaitable<void> process() = 0;

  std::string create_response_to_raw_btes() override {
    std::string ret;
    RpcCreator<response_t>::to_raw_bytes(*response, ret);
    return ret;
  }

 protected:
  std::unique_ptr<request_t> request;
  std::unique_ptr<response_t> response;
};

template <typename RequestType, typename ResponseType, uint32_t pcode> requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcStub {
  using request_t = RequestType;
  using response_t = ResponseType;

 public:
  RpcStub (asio::io_context& io, const std::string& ip, uint16_t port) : request(nullptr), 
                                                                         io_(io),
                                                                         socket_(io_),
                                                                         ip_(ip),
                                                                         port_(port) {

  }

  void connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    asio::connect(socket_, ep);
  }

  asio::awaitable<asio::ip::tcp::endpoint> async_connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    co_return co_await asio::async_connect(socket_, ep, asio::use_awaitable);
  }

  int rpc_call(std::unique_ptr<request_t>&& r, std::unique_ptr<response_t>& response) {
    set_request(std::move(r));
    auto request = create_request_message();
    std::string buf;
    uint32_t length = static_cast<uint32_t>(request.size());
    integralSeri(length, buf);
    buf.append(request);
    asio::write(socket_, asio::buffer(buf));
    auto ret_code = wait_for_response(response);
    return ret_code;
  }

  asio::awaitable<int> rpc_call_coro(std::unique_ptr<request_t>&& r, std::unique_ptr<response_t>& response) {
    set_request(std::move(r));
    auto request = create_request_message();
    std::string buf;
    uint32_t length = static_cast<uint32_t>(request.size());
    integralSeri(length, buf);
    buf.append(request);
    co_await asio::async_write(socket_, asio::buffer(buf), asio::use_awaitable);
    co_return co_await wait_for_response_coro(response);
  }

 private:
  std::unique_ptr<request_t> request;
  asio::io_context& io_;
  asio::ip::tcp::socket socket_;
  std::string ip_;
  uint16_t port_;

  void set_request(std::unique_ptr<request_t>&& r) {
    request = std::move(r);
  }

  std::string create_request_message() {
    std::string ret;
    integralSeri(pcode, ret);
    RpcCreator<request_t>::to_raw_bytes(*request, ret);
    return ret;
  }

  int wait_for_response(std::unique_ptr<response_t>& response) {
    char data[sizeof(uint32_t)];
    asio::read(socket_, asio::buffer(data, sizeof(uint32_t)));
    uint32_t length = integralParse<uint32_t>(&data[0], sizeof(uint32_t));
    std::string buf;
    buf.resize(length);
    asio::read(socket_, asio::buffer(buf));
    return create_response(&buf[0], buf.size(), response);
  }

  asio::awaitable<int> wait_for_response_coro(std::unique_ptr<response_t>& response) {
    char data[sizeof(uint32_t)];
    co_await asio::async_read(socket_, asio::buffer(data, sizeof(uint32_t)), asio::use_awaitable);
    uint32_t length = integralParse<uint32_t>(&data[0], sizeof(uint32_t));
    std::string buf;
    buf.resize(length);
    co_await asio::async_read(socket_, asio::buffer(buf), asio::use_awaitable);
    co_return create_response(&buf[0], buf.size(), response);
  }

  int create_response(const char* ptr, size_t len, std::unique_ptr<response_t>& r) {
    bridge::BridgePool bp;
    std::string tmp(ptr, len);
    auto root = bridge::Parse(tmp, bp);
    bridge::ObjectWrapper wrapper(root.get());
    int ret_code = wrapper["ret_code"].Get<int32_t>().value();
    std::string response = wrapper["response"].Get<std::string>().value();
    if (ret_code == RPC_OK) {
      r = RpcCreator<response_t>::create(&response[0], response.size());
    }
    return ret_code;
  }
};

}

#define RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, ...) \
class RPC ## funcname : public pnrpc::RpcProcessor<request_t, response_t> { \
 public: \
  RPC ## funcname() : pnrpc::RpcProcessor<request_t, response_t>(pcode) {} \
  __VA_ARGS__ \
}; \
\
class RPC ## funcname ## STUB : public pnrpc::RpcStub<request_t, response_t, pcode> { \
 public: \
  RPC ## funcname ## STUB (asio::io_context& io, const std::string& ip, uint16_t port) : pnrpc::RpcStub<request_t, response_t, pcode>(io, ip, port) {} \
}; \

#define OVERRIDE_BIND \
  asio::io_context* bind_io_context() override;

#define OVERRIDE_PROCESS \
  asio::awaitable<void> process() override;

#define OVERRIDE_RESTRICTOR \
  bool restrictor() override;

#define RPC_DECLARE(funcname, request_t, response_t, pcode, ...) \
  RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, __VA_ARGS__)

#define REGISTER_RPC(funcname, pcode) \
  pnrpc::RpcServer::Instance().RegisterRpc(pcode, [](size_t) -> std::unique_ptr<pnrpc::RpcProcessorBase> { \
    return std::make_unique<RPC ## funcname>(); \
  });