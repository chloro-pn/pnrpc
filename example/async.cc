#include "async.h"
#include "echo.h"

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <cassert>
#include <variant>

#include "asio/experimental/awaitable_operators.hpp"
using namespace asio::experimental::awaitable_operators;

struct AsyncTaskHandlerMock {
  using call_back_t = std::function<void(std::string)>;

  AsyncTaskHandlerMock() : stop_(false) {
    backend_ = std::thread([this]() {
      while(stop_.load() == false) {
        std::vector<call_back_t> cbs;
        std::vector<std::string> args;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        mut_.lock();
        cbs_.swap(cbs);
        args_.swap(args);
        mut_.unlock();
        assert(cbs.size() == args.size());
        for(size_t i = 0; i < cbs.size(); ++i) {
          cbs[i](args[i]);
        }
      }
    });
  }

  ~AsyncTaskHandlerMock() {
    stop_.store(true);
    backend_.join();
  }

  void async_request(const std::string& msg, call_back_t&& cb) {
    mut_.lock();
    cbs_.push_back(std::move(cb));
    args_.push_back(msg);
    mut_.unlock();
  }

  std::thread backend_;
  std::atomic<bool> stop_;
  std::mutex mut_;
  std::vector<call_back_t> cbs_;
  std::vector<std::string> args_;
};

asio::awaitable<void> RPCAsync::process() {
  auto request = co_await get_request_arg();
  AsyncTaskHandlerMock at;
  auto request_task = [request_str = request.value(), &at](std::function<void(std::string)>&& rf) mutable -> void {
    at.async_request(request_str, std::move(rf));
  };
  auto response = co_await pnrpc::async_task<std::string>(std::move(request_task));

  RPCEchoSTUB echo_client(get_io_context(), "127.0.0.1", 44444);
  co_await echo_client.async_connect();

  std::string resp;
  std::variant<int, std::monostate> results = co_await (
    echo_client.rpc_call_coro("hello world", resp)
    || asio::steady_timer(get_io_context(), std::chrono::seconds(2)).async_wait(asio::use_awaitable)
  );
  if (results.index() == 0 && std::get<0>(results) == RPC_OK) {
    response = response + ", " + resp;
  }
  co_await set_response_arg(response, true);
  co_return;
}