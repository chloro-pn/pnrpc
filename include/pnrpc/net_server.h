#include "asio/io_context.hpp"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/write.hpp"
#include "asio/read.hpp"
#include "asio/error_code.hpp"

#include "pnrpc/rpc_server.h"

#include <iostream>
#include <exception>
#include <string>

asio::awaitable<void> work(asio::ip::tcp::socket socket) {
  try
  {
    char data[sizeof(uint32_t)];
    for (;;)
    {
      auto tmp = co_await async_read(socket, asio::buffer(data), asio::use_awaitable_t<>());
      const char* ptr = data;
      auto length = integralParse<uint32_t>(ptr, sizeof(uint32_t));
      std::string buf;
      buf.resize(length);
      tmp = co_await async_read(socket, asio::buffer(buf), asio::use_awaitable_t<>());
      std::string response = RpcServer::Instance().HandleRequest(&buf[0], buf.size());
      tmp = co_await async_write(socket, asio::buffer(response), asio::use_awaitable_t<>());
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "echo exception " << e.what() << std::endl;
  }
}


asio::awaitable<void> listener(const std::string& ip, uint16_t port) {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::endpoint ep(asio::ip::address::from_string(ip), port);
  asio::ip::tcp::acceptor acceptor(executor, ep);
  for (;;) {
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable_t<>());
    asio::co_spawn(executor, work(std::move(socket)), asio::detached);
  }
}

class NetServer {
 public:
  NetServer(std::string ip, uint16_t port) : io_(1), ip_(ip), port_(port) {

  }

  void run() {
    try {
    co_spawn(io_, listener(ip_, port_), asio::detached);
    io_.run();
    } catch (std::exception& e) {
      std::cerr << "exception : " << e.what() << std::endl;
    }
  }

 private:
  asio::io_context io_;
  std::string ip_;
  uint16_t port_;
};