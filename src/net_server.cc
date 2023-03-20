#include "pnrpc/net_server.h"

namespace pnrpc {

asio::awaitable<void> work(asio::ip::tcp::socket socket, asio::io_context& io) {
  try {
    for (;;) {
      auto handle_info = co_await RpcServer::Instance().HandleRequest(io, std::move(socket));
      socket = std::move(*handle_info.socket);
      PNRPC_LOG_DEBUG("handle request, pcode = {}, ret_code = {}, process_ms = {}, err_msg = {}, io = {}",
        handle_info.pcode,
        handle_info.ret_code,
        handle_info.process_ms,
        handle_info.err_msg,
        static_cast<void*>(handle_info.bind_ctx));
    }
  }
  catch (asio::system_error& e) {
    if (e.code() == asio::error::eof) {
      //PNRPC_LOG_ERROR("connection is closed by peer");
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
    asio::ip::tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
    if (handle_io.empty()) {
      asio::co_spawn(executor, work(std::move(socket), io), asio::detached);
    } else {
      // 按照轮询的方式将请求分派给handle_io。
      asio::io_context& hio = *handle_io[handle_io_index];
      asio::co_spawn(hio, work(rebind_ctx(std::move(socket), hio), hio), asio::detached);
      handle_io_index += 1;
      if (handle_io_index >= handle_io.size()) {
        handle_io_index = 0;
      }
    }
  }
}

NetServer::NetServer(std::string ip, uint16_t port, size_t io_num) : io_(), handle_io_(), ip_(ip), port_(port) {
  for(size_t i = 0; i < io_num; ++i) {
    handle_io_.emplace_back(std::make_unique<asio::io_context>());
  }
  for(size_t i = 0; i < io_num; ++i) {
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
}