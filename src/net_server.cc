#include "pnrpc/net_server.h"
#include "pnrpc/exception.h"

namespace pnrpc {

net::awaitable<void> work(net::ip::tcp::socket socket, net::io_context& io) {
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
  catch (system_error& e) {
    if (e.code() == net::error::eof) {
      //PNRPC_LOG_ERROR("connection is closed by peer");
    } else {
      PNRPC_LOG_WARN("asio exception : {}", e.what());
      throw;
    }
  }
  // 对于PnrpcException类型的异常而言，不需要向外抛出异常，仅记录日志并结束work协程即可。
  catch (PnrpcException& e) {
    PNRPC_LOG_WARN("rpc exception : {}", e.what());
  }
  catch (std::exception& e)
  {
    PNRPC_LOG_WARN("unknow exception : {}", e.what());
    throw;
  }
}

net::awaitable<void> listener(const std::string& ip, uint16_t port, net::io_context& io, std::vector<std::unique_ptr<net::io_context>>& handle_io) {
  auto executor = co_await net::this_coro::executor;
  net::ip::tcp::endpoint ep(net::ip::address::from_string(ip), port);
  net::ip::tcp::acceptor acceptor(executor, ep);
  size_t handle_io_index = 0;
  for (;;) {
    net::ip::tcp::socket socket = co_await acceptor.async_accept(net::use_awaitable);
    if (handle_io.empty()) {
      net::co_spawn(executor, work(std::move(socket), io), net::detached);
    } else {
      // 按照轮询的方式将请求分派给handle_io。
      net::io_context& hio = *handle_io[handle_io_index];
      net::co_spawn(hio, work(rebind_ctx(std::move(socket), hio), hio), net::detached);
      handle_io_index += 1;
      if (handle_io_index >= handle_io.size()) {
        handle_io_index = 0;
      }
    }
  }
}

NetServer::NetServer(std::string ip, uint16_t port, size_t io_num) : io_(), handle_io_(), ip_(ip), port_(port) {
  for(size_t i = 0; i < io_num; ++i) {
    handle_io_.emplace_back(std::make_unique<net::io_context>());
  }
  for(size_t i = 0; i < io_num; ++i) {
    handle_thread_.emplace_back(std::thread([this, i]()->void {
      try {
        auto work = net::make_work_guard(*this->handle_io_[i]);
        this->handle_io_[i]->run();
        PNRPC_LOG_INFO("NetServer's handle_io_ {} stop.", i);
      } catch (std::exception& e) {
        PNRPC_LOG_ERROR("unknow exception : {}", e.what());
      }
    }));
  }
}
}