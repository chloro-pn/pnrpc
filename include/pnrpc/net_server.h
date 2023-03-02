#include "asio.hpp"

#include "pnrpc/rpc_server.h"
#include "pnrpc/util.h"
#include "pnrpc/log.h"

#include <exception>
#include <string>
#include <vector>
#include <thread>

namespace pnrpc {

asio::awaitable<void> work(asio::ip::tcp::socket socket, asio::io_context& io) {
  try
  {
    char data[sizeof(uint32_t)];
    for (;;)
    {
      Timer timer;
      timer.Start();
      co_await asio::async_read(socket, asio::buffer(data), asio::use_awaitable_t<>());
      const char* ptr = data;
      auto length = integralParse<uint32_t>(ptr, sizeof(uint32_t));
      PNRPC_LOG_INFO("get request, legnth : {}", length);
      std::string buf;
      buf.resize(length);
      co_await asio::async_read(socket, asio::buffer(buf), asio::use_awaitable_t<>());
      PNRPC_LOG_INFO("read request message use {} ms", timer.End());
      timer.Start();
      std::string response = co_await RpcServer::Instance().HandleRequest(&buf[0], buf.size(), io);
      PNRPC_LOG_INFO("handle request use {} ms", timer.End());
      std::string msg;
      length = response.size();
      integralSeri(length, msg);
      msg.append(std::move(response));
      co_await asio::async_write(socket, asio::buffer(msg), asio::use_awaitable_t<>());
    }
  }
  catch (asio::system_error& e) {
    if (e.code() == asio::error::eof) {
      PNRPC_LOG_INFO("connection is closed by peer");
    } else {
      PNRPC_LOG_WARN("rpc handle exception : {}", e.what());
    }
  }
  catch (std::exception& e)
  {
    PNRPC_LOG_WARN("rpc handle exception : {}", e.what());
  }
}


asio::awaitable<void> listener(const std::string& ip, uint16_t port, asio::io_context& io, std::vector<std::unique_ptr<asio::io_context>>& handle_io) {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::endpoint ep(asio::ip::address::from_string(ip), port);
  asio::ip::tcp::acceptor acceptor(executor, ep);
  size_t handle_io_index = 0;
  for (;;) {
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable_t<>());
    if (handle_io.empty()) {
      asio::co_spawn(executor, work(std::move(socket), io), asio::detached);
    } else {
      asio::io_context& hio = *handle_io[handle_io_index];
      asio::co_spawn(hio, work(std::move(socket), hio), asio::detached);
      handle_io_index += 1;
      if (handle_io_index >= handle_io.size()) {
        handle_io_index = 0;
      }
    }
  }
}

class NetServer {
 public:
  NetServer(std::string ip, uint16_t port, size_t io_num = 0) : io_(), handle_io_(), ip_(ip), port_(port) {
    for(size_t i = 0; i < io_num; ++i) {
      handle_io_.emplace_back(std::make_unique<asio::io_context>());
      handle_thread_.emplace_back(std::thread([this, i]()->void {
        try {
          auto work = asio::make_work_guard(*this->handle_io_[i]);
          this->handle_io_[i]->run();
          PNRPC_LOG_INFO("NetServer's handle_io_ {} stop.", i);
        } catch (std::exception& e) {
          PNRPC_LOG_ERROR("NetServer's handle thread exception : {}", e.what());
        }
      }));
    }
  }

  void run() {
    try {
      co_spawn(io_, listener(ip_, port_, io_, handle_io_), asio::detached);
      io_.run();
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("NetServer.run() exception : {}", e.what());
    }
    PNRPC_LOG_INFO("net server stop");
  }

  void stop() {
    for(auto& each : handle_io_) {
      each->stop();
    }
    // 需要等所有handle_io_ stop之后io_才可以stop.
    io_.stop();
    for(auto& each : handle_thread_) {
      each.join();
    }
  }

  ~NetServer() {
    PNRPC_LOG_INFO("NetServer destructor");
  }

 private:
  asio::io_context io_;
  std::vector<std::unique_ptr<asio::io_context>> handle_io_;
  std::vector<std::thread> handle_thread_;
  std::string ip_;
  uint16_t port_;
};

}