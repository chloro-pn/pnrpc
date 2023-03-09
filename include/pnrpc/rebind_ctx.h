#pragma once

#include "asio.hpp"

namespace pnrpc {

inline asio::ip::tcp::socket rebind_ctx(asio::ip::tcp::socket s, asio::io_context& io) {
  auto fd = s.release();
  asio::ip::tcp::socket s2(io);
  s2.assign(asio::ip::tcp::v4(), fd);
  return s2;
}

}