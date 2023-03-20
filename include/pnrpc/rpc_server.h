#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <string_view>

#include "pnrpc/rpc_concept.h"
#include "pnrpc/rpc_ret_code.h"
#include "pnrpc/util.h"
#include "pnrpc/log.h"
#include "pnrpc/rebind_ctx.h"
#include "pnrpc/rpc_type_creator.h"
#include "pnrpc/stream.h"
#include "bridge/object.h"
#include "asio.hpp"

#include "asio/dispatch.hpp"

namespace pnrpc {

// 由使用者保证，在RpcProcessorBase类型的对象生命周期内，running_io_总是有效的。
class RpcProcessorBase {
 public:
  RpcProcessorBase(size_t pcode, RpcType rt) : code(pcode), rpc_type_(rt), running_io_(nullptr) {

  }

  virtual void create_request_from_raw_bytes(std::string_view request_view, bool eof) = 0;
  virtual asio::awaitable<void> process() = 0;

  void set_io_context(asio::io_context& io) {
    running_io_ = &io;
  }

  asio::io_context& get_io_context() {
    return *running_io_;
  }

  virtual void bind_net(asio::ip::tcp::socket& s, asio::io_context& io_context) = 0;

  size_t get_pcode() const { return code; }

  RpcType get_rpc_type() const { return rpc_type_; }

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
  RpcType rpc_type_;
  asio::io_context* running_io_;
};

class RpcServer {
 public:
  using CreatorFunction = std::function<std::unique_ptr<RpcProcessorBase>()>;

  struct HandleInfo {
    int ret_code = RPC_OK;
    std::string err_msg = "";
    uint32_t pcode = 0;
    double process_ms = 0.0;
    asio::io_context* bind_ctx = nullptr;
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

  asio::awaitable<HandleInfo> HandleRequest(asio::io_context& io, asio::ip::tcp::socket socket) {
    HandleInfo handle_info;

    ClientToServerStream<void> ctss;
    ctss.update_bind_socket(&socket);
    auto request_view = co_await ctss.Read();
    auto processor = GetProcessor(ctss.get_pcode());
    handle_info.pcode = ctss.get_pcode();
    if (processor == nullptr) {
      handle_info.ret_code = RPC_INVALID_PCODE;
      handle_info.err_msg = "not found rpc request, pcode == " + std::to_string(handle_info.pcode);
    } else {
      // 在调用bind_io_context之前解析请求，这意味着可以根据请求信息做更细粒度的分流。
      processor->create_request_from_raw_bytes(request_view, ctss.get_eof());
      processor->bind_net(socket, io);
      handle_info.bind_ctx = &io;
      auto bind_ctx = processor->bind_io_context();
      // 如果用户给该rpc绑定了io_context，则将本协程调度给该io_context执行，注意，需要将socket绑定的ctx和processor注册的ctx同步修改
      if (bind_ctx != nullptr) {
        handle_info.bind_ctx = bind_ctx;
        socket = rebind_ctx(std::move(socket), *bind_ctx);
        processor->bind_net(socket, *bind_ctx);
        co_await asio::dispatch(asio::bind_executor(*bind_ctx, asio::use_awaitable));
      }
      // 在调度到执行本rpc的io_context上之后进行限流判定，这意味着可以通过请求信息、io_context信息等做更细粒度的限流
      bool overload = !processor->restrictor();
      if (overload) {
        handle_info.ret_code = RPC_OVERFLOW;
        handle_info.err_msg = "rpc request overflow";
      } else {
        Timer timer;
        timer.Start();
        co_await processor->process();
        handle_info.process_ms = timer.End();
        handle_info.ret_code = RPC_OK;
        handle_info.err_msg = "";
      }
    }
    if (handle_info.ret_code != RPC_OK) {
      ErrorStream es;
      es.update_bind_socket(&socket);
      co_await es.SendErrorMsg(handle_info.err_msg, handle_info.ret_code);
    }
    handle_info.socket = std::make_unique<asio::ip::tcp::socket>(std::move(socket));
    co_return handle_info;
  }

  std::unique_ptr<RpcProcessorBase> GetProcessor(size_t pcode) {
    auto it = funcs_.find(pcode);
    if (it == funcs_.end()) {
      PNRPC_LOG_INFO("get processor failed, pcode = {}", pcode);
      return nullptr;
    }
    return it->second();
  }

 private:
  std::unordered_map<size_t, CreatorFunction> funcs_;

  RpcServer() {}
};

template <typename RequestType, typename ResponseType, uint32_t c, RpcType rpc_type> requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcProcessor : public RpcProcessorBase {
  using request_t = RequestType;
  using response_t = ResponseType;

 public:
  static constexpr uint32_t pcode = c;

  explicit RpcProcessor() : RpcProcessorBase(pcode, rpc_type), request_stream(pcode), response_stream(), request_count_(0), response_eof_(false), response_count_(0) {}

  void create_request_from_raw_bytes(std::string_view request_view, bool eof) override {
    auto request = RpcCreator<request_t>::create(&request_view[0], request_view.size());
    request_stream.set_init_package(std::move(request), eof);
  }

  virtual asio::awaitable<void> process() = 0;

  void bind_net(asio::ip::tcp::socket& s, asio::io_context& io_context) override {
    request_stream.update_bind_socket(&s);
    response_stream.update_bind_socket(&s);
    set_io_context(io_context);
  }

  asio::awaitable<std::unique_ptr<request_t>> get_request_arg() {
    if (get_rpc_type() == RpcType::Simple || get_rpc_type() == RpcType::ServerSideStream) {
      if (request_count_ >= 1) {
        PNRPC_LOG_WARN("rpc {} request_count_ == {}", pcode, request_count_);
      }
      co_return nullptr;
    }
    request_count_ += 1;
    co_return co_await get_request_stream().Read();
  }

  asio::awaitable<void> set_response_arg(std::unique_ptr<response_t>&& r, bool eof) {
    if (response_eof_ == true) {
      PNRPC_LOG_WARN("rpc {} repeatedly set eof", pcode);
      co_return;
    }
    if (get_rpc_type() == RpcType::Simple || get_rpc_type() == RpcType::ClientSideStream) {
      if (response_count_ >= 1) {
        PNRPC_LOG_WARN("rpc {} response_count_ == {}", pcode, response_count_);
      }
    }
    response_eof_ = eof;
    response_count_ += 1;
    co_return co_await get_response_stream().Send(std::move(r), RPC_OK, eof);
  }

  ~RpcProcessor() {
    if (response_eof_ == false) {
      PNRPC_LOG_WARN("rpc {} diden't send eof response", pcode);
    }
  }

 protected:
  ClientToServerStream<request_t>& get_request_stream() {
    return request_stream;
  }

  ServerToClientStream<response_t>& get_response_stream() {
    return response_stream;
  }

 private:
  ClientToServerStream<request_t> request_stream;
  ServerToClientStream<response_t> response_stream;
  // 接收请求包的个数
  size_t request_count_;
  // 是否回复eof
  bool response_eof_;
  // 发送回复包的个数
  size_t response_count_;
};


template <typename RequestType, typename ResponseType, uint32_t pcode> requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcStubBase {
 public:
   using request_t = RequestType;
   using response_t = ResponseType;

   RpcStubBase(asio::io_context& io, const std::string& ip, uint16_t port) : io_(io),
                                                                             socket_(io_),
                                                                             ip_(ip),
                                                                             port_(port),
                                                                             request_stream(pcode),
                                                                             response_stream() {
    request_stream.update_bind_socket(&socket_);
    response_stream.update_bind_socket(&socket_);
   }

  asio::awaitable<asio::ip::tcp::endpoint> async_connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    co_return co_await asio::async_connect(socket_, ep, asio::use_awaitable);
  }

  asio::ip::tcp::endpoint connect() {
    asio::ip::tcp::resolver resolver(io_);
    auto ep = resolver.resolve(ip_, std::to_string(port_));
    return asio::connect(socket_, ep);
  }

 private:
  asio::io_context& io_;
  asio::ip::tcp::socket socket_;
  std::string ip_;
  uint16_t port_;

 protected:
  ClientToServerStream<request_t> request_stream;
  ServerToClientStream<response_t> response_stream;
};

template <typename RequestType, typename ResponseType, uint32_t pcode, RpcType rpc_type>
class RpcStub : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;

  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port) {}

  asio::awaitable<int> rpc_call_coro(std::unique_ptr<request_t>&& r, std::unique_ptr<response_t>& response) {
    co_await this->request_stream.Send(std::move(r), true);
    uint32_t ret_code = 0;
    std::string err_msg;
    response = co_await this->response_stream.Read(ret_code, err_msg);
    if (ret_code != RPC_OK) {
      PNRPC_LOG_INFO("rpc call failed, ret_code = {}, err_msg = {}", ret_code, err_msg);
    }
    co_return ret_code;
  }
};

template <typename RequestType, typename ResponseType, uint32_t pcode>
class RpcStub<RequestType, ResponseType, pcode, RpcType::ClientSideStream> : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;

  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port), send_eof_(false), recved_(false) {}

  asio::awaitable<int> send_request(std::unique_ptr<request_t>&& request, bool eof = false) {
    if (send_eof_ == true) {
      co_return RPC_SEND_AFTER_EOF;
    }
    co_await this->request_stream.Send(std::move(request), eof);
    send_eof_ = eof;
    co_return RPC_OK;
  }

  asio::awaitable<int> recv_response(std::unique_ptr<response_t>& response) {
    if (send_eof_ == false) {
      co_return  RPC_RECV_BEFORE_EOF;
    }
    if (recved_ == true) {
      co_return RPC_RECV_DUPLICATE;
    }
    uint32_t ret_code = 0;
    std::string err_msg;
    response = co_await this->response_stream.Read(ret_code, err_msg);
    recved_ = true;
    if (ret_code != RPC_OK) {
      PNRPC_LOG_WARN("rpc response error : {}, {}", ret_code, err_msg);
    }
    co_return ret_code;
  }

 private:
  bool send_eof_;
  bool recved_;
};

template <typename RequestType, typename ResponseType, uint32_t pcode>
class RpcStub<RequestType, ResponseType, pcode, RpcType::ServerSideStream> : public RpcStubBase<RequestType, ResponseType, pcode> {
 public:
  using request_t = typename RpcStubBase<RequestType, ResponseType, pcode>::request_t;
  using response_t = typename RpcStubBase<RequestType, ResponseType, pcode>::response_t;
  RpcStub(asio::io_context& io, const std::string& ip, uint16_t port) : RpcStubBase<RequestType, ResponseType, pcode>(io, ip, port), send_eof_(false) {}

  asio::awaitable<int> send_request(std::unique_ptr<request_t>&& request) {
    if (send_eof_ == true) {
      co_return RPC_SEND_AFTER_EOF;
    }
    send_eof_ = true;
    co_await this->request_stream.Send(std::move(request), send_eof_);
    co_return RPC_OK;
  }

  asio::awaitable<int> recv_response(std::unique_ptr<response_t>& response) {
    if (send_eof_ == false) {
      co_return  RPC_RECV_BEFORE_EOF;
    }
    uint32_t ret_code = 0;
    std::string err_msg;
    response = co_await this->response_stream.Read(ret_code, err_msg);
    if (ret_code != RPC_OK) {
      PNRPC_LOG_WARN("rpc response error : {}, {}", ret_code, err_msg);
    }
    co_return ret_code;
  }

 private:
  bool send_eof_;
};

}

#define RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, rpc_type, ...) \
class RPC ## funcname : public pnrpc::RpcProcessor<request_t, response_t, pcode, rpc_type> { \
 public: \
  RPC ## funcname() : pnrpc::RpcProcessor<request_t, response_t, pcode, rpc_type>() {} \
  __VA_ARGS__ \
}; \
\
class RPC ## funcname ## STUB : public pnrpc::RpcStub<request_t, response_t, pcode, rpc_type> { \
 public: \
  RPC ## funcname ## STUB (asio::io_context& io, const std::string& ip, uint16_t port) : pnrpc::RpcStub<request_t, response_t, pcode, rpc_type>(io, ip, port) {} \
}; \

#define OVERRIDE_BIND \
  asio::io_context* bind_io_context() override;

#define OVERRIDE_PROCESS \
  asio::awaitable<void> process() override;

#define OVERRIDE_RESTRICTOR \
  bool restrictor() override;

#define RPC_DECLARE(funcname, request_t, response_t, pcode, rpc_type, ...) \
  RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, rpc_type, __VA_ARGS__)

#define REGISTER_RPC(funcname) \
  pnrpc::RpcServer::Instance().RegisterRpc(RPC ## funcname::pcode, []() -> std::unique_ptr<pnrpc::RpcProcessorBase> { \
    return std::make_unique<RPC ## funcname>(); \
  });