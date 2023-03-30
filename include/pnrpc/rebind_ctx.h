#pragma once

#include "pnrpc/asio_version.h"

namespace pnrpc {

inline net::ip::tcp::socket rebind_ctx(net::ip::tcp::socket s, net::io_context& io) {
  auto fd = s.release();
  net::ip::tcp::socket s2(io);
  s2.assign(net::ip::tcp::v4(), fd);
  return s2;
}

}  // namespace pnrpc