/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include "common.h"

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/redis/src.hpp>
#include <iostream>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using resolver = net::use_awaitable_t<>::as_default_on_t<net::ip::tcp::resolver>;
using timer_type = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using boost::redis::operation;
using boost::redis::request;

namespace {
auto redir(boost::system::error_code& ec) { return net::redirect_error(net::use_awaitable, ec); }
}  // namespace

auto connect(std::shared_ptr<connection> conn, std::string const& host, std::string const& port)
    -> net::awaitable<void> {
  auto ex = co_await net::this_coro::executor;
  resolver resv{ex};
  timer_type timer{ex};

  boost::system::error_code ec;
  timer.expires_after(std::chrono::seconds{5});
  auto const addrs = co_await (resv.async_resolve(host, port) || timer.async_wait(redir(ec)));
  if (!ec) throw std::runtime_error("Resolve timeout");

  timer_type t2{ex};
  t2.expires_after(std::chrono::seconds{5});
  co_await (net::async_connect(conn->next_layer(), std::get<0>(addrs)) || t2.async_wait(redir(ec)));
  if (!ec) {
    throw std::runtime_error("Connect timeout");
  }
}

auto start(net::awaitable<void> op) -> int {
  try {
    net::io_context ioc;
    net::co_spawn(ioc, std::move(op), net::detached);
    ioc.run();

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 1;
}