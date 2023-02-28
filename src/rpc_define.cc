#include <string>
#include <thread>

#include <functional>

#include "asio.hpp"
#include "pnrpc/rpc_define.h"

asio::awaitable<void> RPCdesc::process() {
  response.reset(new std::string());
  *response = request->str + ", your age is " + std::to_string(request->age) + ", and you " + (request->man == 1 ? "are " : "are not ") + "a man";
  co_return;
}

asio::awaitable<void> RPCnum_add::process() {
  response.reset(new uint32_t);
  *response = *request * 10;
  co_return;
}

template <typename ReturnType>
auto async_task(std::function<void(std::function<void(ReturnType)>&&)> request_task) {
  auto initiate = [request_task = std::move(request_task)]<typename Handler>(Handler&& handler) mutable {
    auto shared_handler = std::make_shared<Handler>(std::forward<Handler>(handler));
    auto&& resume_func = 
      [shared_handler](ReturnType rt) mutable -> void  {
        auto ex = asio::get_associated_executor(*shared_handler);
        asio::dispatch(ex, [shared_handler, rt = std::move(rt)]() mutable {
          (*shared_handler)(rt);
        });
      };
      request_task(std::move(resume_func));
  };
  auto h = asio::use_awaitable_t<>();
  return asio::async_initiate<asio::use_awaitable_t<>, void(ReturnType)>(std::move(initiate), h);
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

// 这个rpc首先将异步操作封装为一个awaiter并co_await等其完成，然后新建client请求了另一个rpc方法（client支持阻塞式和协程挂起式的rpc请求，这里使用了协程版本）
// 所有的网络io都是非阻塞的
asio::awaitable<void> RPCasync_task::process() {
  response.reset(new std::string());
  auto request_task = [request_str = *request](std::function<void(std::string)>&& rf) mutable -> void {
    AsyncTaskHandlerMock::Instance().push_async_task(request_str, std::move(rf));
  };
  std::string tmp = co_await std::move(async_task<std::string>(std::move(request_task)));
  // just for test 
  RPCnum_addSTUB client(get_io_context(), "127.0.0.1", 44444);
  auto add_req = std::make_unique<uint32_t>(10);
  auto add_resp = std::make_unique<uint32_t>();
  int ret_code = co_await client.rpc_call_coro(std::move(add_req), add_resp);
  *response = tmp;
  if (ret_code == RPC_OK) {
    *response += std::to_string(*add_resp);
  }
  co_return;
}