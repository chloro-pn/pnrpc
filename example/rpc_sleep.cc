#include "rpc_sleep.h"

asio::io_context global_default_ctx;

asio::io_context* RPCSleep::bind_io_context() {
  return &global_default_ctx;
}

asio::awaitable<void> RPCSleep::process() {
  PNRPC_LOG_INFO("my handle io's address is {}", static_cast<void*>(&get_io_context()));
  response.reset(new uint32_t());
  std::chrono::seconds s(*request);
  asio::steady_timer timer(get_io_context());
  timer.expires_after(s);
  co_await timer.async_wait(asio::use_awaitable);
  *response = *request;
  co_return;
}