#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bridge/object.h"
#include "pnrpc/asio_version.h"
#include "pnrpc/log.h"
#include "pnrpc/rebind_ctx.h"
#include "pnrpc/rpc_concept.h"
#include "pnrpc/rpc_ret_code.h"
#include "pnrpc/rpc_type_creator.h"
#include "pnrpc/stream.h"
#include "pnrpc/util.h"

namespace pnrpc {

class RpcServer;

// 由使用者保证，在RpcProcessorBase类型的对象生命周期内，running_io_总是有效的。
class RpcProcessorBase {
  friend class RpcServer;

 public:
  RpcProcessorBase(size_t pcode, RpcType rt) : code(pcode), rpc_type_(rt), running_io_(nullptr) {}

  net::io_context& get_io_context() {
    assert(running_io_ != nullptr);
    return *running_io_;
  }

  size_t get_pcode() const { return code; }

  RpcType get_rpc_type() const { return rpc_type_; }

 protected:
  void set_io_context(net::io_context& io) { running_io_ = &io; }

  virtual void create_request_from_raw_bytes(std::string_view request_view, bool eof) = 0;

  virtual net::awaitable<void> process() = 0;

  virtual void bind_net(net::ip::tcp::socket& s, net::io_context& io_context) = 0;

  // 用户可以通过重写此方法将本rpc分配给自定义的handle_io处理
  virtual net::io_context* bind_io_context() { return nullptr; }

  // 用户可以通过重写此方法对从客户端读取数据做限流, 单位 字节 / 秒
  virtual size_t get_request_current_limiting() { return 0; }

  // 用户可以通过重写此方法对向客户端写回数据做限流, 单位 字节 / 秒
  virtual size_t get_response_current_limiting() { return 0; }

  // 用户可以通过重写此方法实现限流算法。注意：
  // 每个RpcProcessorBase对象只负责处理一次rpc请求，因此需要将限流信息存储在生命周期更长的对象中而不是RpcProcessorBase对象中。
  virtual bool restrictor() { return true; }

  virtual void update_request_current_limiting(size_t uwl) = 0;

  virtual void update_response_current_limiting(size_t uwl) = 0;

 private:
  size_t code;
  RpcType rpc_type_;
  net::io_context* running_io_;
};

class RpcServer {
 public:
  using CreatorFunction = std::function<std::unique_ptr<RpcProcessorBase>()>;

  struct HandleInfo {
    int ret_code = RPC_OK;
    std::string err_msg = "";
    uint32_t pcode = 0;
    double process_ms = 0.0;
    net::io_context* bind_ctx = nullptr;
    std::unique_ptr<net::ip::tcp::socket> socket;
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

  net::awaitable<HandleInfo> HandleRequest(net::io_context& io, net::ip::tcp::socket socket) {
    HandleInfo handle_info;
    ClientToServerStream<void> ctss;
    ctss.update_bind_socket(&socket);
    std::string buf;
    auto request_view = co_await ctss.Read(buf);
    auto processor = GetProcessor(ctss.get_pcode());
    handle_info.pcode = ctss.get_pcode();
    if (processor == nullptr) {
      handle_info.ret_code = RPC_INVALID_PCODE;
      handle_info.err_msg = "not found rpc request, pcode == " + std::to_string(handle_info.pcode);
    } else {
      // 设置socket限流
      processor->update_request_current_limiting(processor->get_request_current_limiting());
      processor->update_response_current_limiting(processor->get_response_current_limiting());
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
        co_await net::dispatch(net::bind_executor(*bind_ctx, net::use_awaitable));
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
    handle_info.socket = std::make_unique<net::ip::tcp::socket>(std::move(socket));
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

template <typename RequestType, typename ResponseType, uint32_t c, RpcType rpc_type>
requires RpcTypeConcept<RequestType> && RpcTypeConcept<ResponseType>
class RpcProcessor : public RpcProcessorBase {
  using request_t = RequestType;
  using response_t = ResponseType;

 protected:
  void create_request_from_raw_bytes(std::string_view request_view, bool eof) override {
    auto request = RpcCreator<request_t>::create(&request_view[0], request_view.size());
    first_requset_pkg_.reset(new request_t(std::move(request)));
    first_read_eof_ = eof;
  }

  virtual net::awaitable<void> process() = 0;

  void bind_net(net::ip::tcp::socket& s, net::io_context& io_context) override {
    request_stream.update_bind_socket(&s);
    response_stream.update_bind_socket(&s);
    set_io_context(io_context);
  }

  void update_request_current_limiting(size_t uwl) override { request_stream.update_read_limiting(uwl); }

  void update_response_current_limiting(size_t uwl) override { response_stream.update_write_limiting(uwl); }

 public:
  static constexpr uint32_t pcode = c;

  explicit RpcProcessor()
      : RpcProcessorBase(pcode, rpc_type),
        request_stream(pcode),
        response_stream(),
        request_count_(0),
        response_eof_(false),
        response_count_(0),
        first_requset_pkg_(nullptr),
        first_read_eof_(false) {}

  net::awaitable<std::optional<request_t>> get_request_arg() {
    if (get_rpc_type() == RpcType::Simple || get_rpc_type() == RpcType::ServerSideStream) {
      if (request_count_ >= 1) {
        PNRPC_LOG_WARN("rpc {} request_count_ == {}", pcode, request_count_);
        co_return std::optional<request_t>();
      }
    }
    request_count_ += 1;
    if (first_requset_pkg_ != nullptr) {
      auto tmp = std::move(first_requset_pkg_);
      co_return std::optional<request_t>(std::move(*tmp));
    }
    if (first_read_eof_ == true) {
      co_return std::optional<request_t>();
    }
    co_return co_await get_request_stream().Read();
  }

  net::awaitable<void> set_response_arg(const response_t& r, bool eof) {
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
    co_await get_response_stream().Send(r, RPC_OK, eof);
    co_return;
  }

  ~RpcProcessor() {
    if (response_eof_ == false) {
      PNRPC_LOG_WARN("rpc {} diden't send eof response", pcode);
    }
  }

 protected:
  ClientToServerStream<request_t>& get_request_stream() { return request_stream; }

  ServerToClientStream<response_t>& get_response_stream() { return response_stream; }

 private:
  ClientToServerStream<request_t> request_stream;
  ServerToClientStream<response_t> response_stream;
  // 接收请求包的个数
  size_t request_count_;
  // 是否回复eof
  bool response_eof_;
  // 发送回复包的个数
  size_t response_count_;

  std::unique_ptr<request_t> first_requset_pkg_;
  bool first_read_eof_;
};

}  // namespace pnrpc