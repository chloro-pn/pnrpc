#include "asio.hpp"

#include "pnrpc/rpc_server.h"
#include "pnrpc/log.h"

#include <exception>
#include <string>

namespace pnrpc {

asio::awaitable<void> work(asio::ip::tcp::socket socket, asio::io_context& io) {
  try
  {
    char data[sizeof(uint32_t)];
    for (;;)
    {
      co_await asio::async_read(socket, asio::buffer(data), asio::use_awaitable_t<>());
      const char* ptr = data;
      auto length = integralParse<uint32_t>(ptr, sizeof(uint32_t));
      PNRPC_LOG_INFO("get request, legnth : {}", length);
      std::string buf;
      buf.resize(length);
      co_await asio::async_read(socket, asio::buffer(buf), asio::use_awaitable_t<>());
      std::string response = co_await RpcServer::Instance().HandleRequest(&buf[0], buf.size(), io);

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
    PNRPC_LOG_INFO("rpc handle exception : {}", e.what());
  }
}


asio::awaitable<void> listener(const std::string& ip, uint16_t port, asio::io_context& io) {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::endpoint ep(asio::ip::address::from_string(ip), port);
  asio::ip::tcp::acceptor acceptor(executor, ep);
  for (;;) {
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable_t<>());
    asio::co_spawn(executor, work(std::move(socket), io), asio::detached);
  }
}

class NetServer {
 public:
  NetServer(std::string ip, uint16_t port) : io_(1), ip_(ip), port_(port) {

  }

  void run() {
    try {
      co_spawn(io_, listener(ip_, port_, io_), asio::detached);
      io_.run();
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("exception : {}", e.what());
    }
    PNRPC_LOG_INFO("net server stop");
  }

  void stop() {
    io_.stop();
  }

 private:
  asio::io_context io_;
  std::string ip_;
  uint16_t port_;
};

}