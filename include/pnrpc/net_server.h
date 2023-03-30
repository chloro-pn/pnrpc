#pragma once

#include <exception>
#include <string>
#include <thread>
#include <vector>

#include "pnrpc/asio_version.h"
#include "pnrpc/exception.h"
#include "pnrpc/log.h"
#include "pnrpc/rebind_ctx.h"
#include "pnrpc/rpc_server.h"
#include "pnrpc/util.h"

namespace pnrpc {

net::awaitable<void> work(net::ip::tcp::socket socket, net::io_context& io);

net::awaitable<void> listener(const std::string& ip, uint16_t port, net::io_context& io,
                              std::vector<std::unique_ptr<net::io_context>>& handle_io);

class NetServer {
 public:
  NetServer(std::string ip, uint16_t port, size_t io_num = 0);

  void run() {
    try {
      co_spawn(io_, listener(ip_, port_, io_, handle_io_), net::detached);
      io_.run();
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("unknow exception : {}", e.what());
      throw;
    }
    PNRPC_LOG_INFO("net server stop");
  }

  void stop() {
    for (auto& each : handle_io_) {
      each->stop();
    }
    for (auto& each : handle_thread_) {
      each.join();
    }
    // 需要等所有handle_io_ stop之后io_才可以stop.
    io_.stop();
  }

  ~NetServer() {}

 private:
  net::io_context io_;
  std::vector<std::unique_ptr<net::io_context>> handle_io_;
  std::vector<std::thread> handle_thread_;
  std::string ip_;
  uint16_t port_;
};

}  // namespace pnrpc