#include <string>
#include <thread>
#include <variant>
#include <functional>
#include <iostream>
#include <chrono>

#include "asio.hpp"
#include <asio/experimental/awaitable_operators.hpp>
#include "pnrpc/rpc_define.h"
#include "pnrpc/async_task.h"

asio::awaitable<void> RPCEcho::process() {
  response.reset(new std::string(*request));
  co_return;
}

asio::awaitable<void> RPCDesc::process() {
  response.reset(new std::string());
  *response = request->str + ", your age is " + std::to_string(request->age) + ", and you " + (request->man == 1 ? "are " : "are not ") + "a man";
  PNRPC_LOG_INFO("my handle io's address is {}", static_cast<void*>(&get_io_context()));
  co_return;
}

asio::awaitable<void> RPCSleep::process() {
  response.reset(new uint32_t());
  std::chrono::seconds s(*request);
  asio::steady_timer timer(get_io_context());
  timer.expires_after(s);
  co_await timer.async_wait(asio::use_awaitable);
  *response = *request;
  co_return;
}

struct AsyncTaskHandlerMock {
  using call_back_t = std::function<void(std::string)>;
  void push_async_task(const std::string& msg, call_back_t&& cb) {
    auto ret = msg + msg;
    cb(ret);
  }

  static AsyncTaskHandlerMock& Instance() {
    static AsyncTaskHandlerMock obj;
    return obj;
  }
};

using namespace asio::experimental::awaitable_operators;

/*
 * 可以无缝使用asio提供的协程基础设施（socket、timer、Error Handling、Co-ordinating Parallel Coroutines等等）；
 * 可以将异步操作封装为awaiter并在rpc处理函数（也是一个协程）中使用；
 * 客户端stub的rpc请求提供了协程版本，因此可以在rpc处理函数中对其他rpc服务发起调用，一切都是非阻塞的；
 */
asio::awaitable<void> RPCasync_task::process() {
  response.reset(new std::string());
  auto request_task = [request_str = *request](std::function<void(std::string)>&& rf) mutable -> void {
    AsyncTaskHandlerMock::Instance().push_async_task(request_str, std::move(rf));
  };
  *response = co_await pnrpc::async_task<std::string>(std::move(request_task));

  RPCSleepSTUB client(get_io_context(), "127.0.0.1", 44444);
  auto add_req = std::make_unique<uint32_t>(1);
  auto add_resp = std::make_unique<uint32_t>();

  asio::steady_timer timer(get_io_context());
  timer.expires_after(std::chrono::seconds(3));

  std::variant<int, std::monostate> results = co_await (
    client.rpc_call_coro(std::move(add_req), add_resp) ||
    timer.async_wait(asio::use_awaitable)
  );
  if (results.index() != 0) {
    *response += ", sleep rpc request timeout";
  } else {
    int ret_code = std::get<int>(results);
    if (ret_code == RPC_OK) {
      *response += ", sleep rpc response " + std::to_string(*add_resp);
    }
  }
  co_return;
}