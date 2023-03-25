#pragma once

#include "asio.hpp"

#include "pnrpc/rpc_server.h"
#include "pnrpc/util.h"
#include "pnrpc/log.h"
#include "pnrpc/rebind_ctx.h"
#include "pnrpc/exception.h"

#include <exception>
#include <string>
#include <vector>
#include <thread>

namespace pnrpc {

asio::awaitable<void> work(asio::ip::tcp::socket socket, asio::io_context& io);


asio::awaitable<void> listener(const std::string& ip, uint16_t port, asio::io_context& io, std::vector<std::unique_ptr<asio::io_context>>& handle_io);

class NetServer {
 public:
  NetServer(std::string ip, uint16_t port, size_t io_num = 0);

  void run() {
    try {
      co_spawn(io_, listener(ip_, port_, io_, handle_io_), asio::detached);
      io_.run();
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("unknow exception : {}", e.what());
      throw;
    }
    PNRPC_LOG_INFO("net server stop");
  }

  void stop() {
    for(auto& each : handle_io_) {
      each->stop();
    }
    for(auto& each : handle_thread_) {
      each.join();
    }
    // 需要等所有handle_io_ stop之后io_才可以stop.
    io_.stop();
  }

  ~NetServer() {
    
  }

 private:
  asio::io_context io_;
  std::vector<std::unique_ptr<asio::io_context>> handle_io_;
  std::vector<std::thread> handle_thread_;
  std::string ip_;
  uint16_t port_;
};

}