#include "rpc_sleep.h"

std::unique_ptr<asio::io_context> global_default_ctx;

asio::io_context* RPCSleep::bind_io_context() {
  return global_default_ctx.get();
}


asio::awaitable<void> RPCSleep::process() {
  PNRPC_LOG_INFO("my handle io's address is {}", static_cast<void*>(&get_io_context()));
  auto request = co_await get_request_arg();
  std::chrono::seconds s(*request);
  asio::steady_timer timer(get_io_context());
  timer.expires_after(s);
  co_await timer.async_wait(asio::use_awaitable);
  auto response = std::make_unique<uint32_t>(*request);
  co_await set_response_arg(std::move(response), true);
  co_return;
}