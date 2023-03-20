#include "echo.h"

asio::awaitable<void> RPCEcho::process() {
  auto request = co_await get_request_arg();
  co_await set_response_arg(std::move(request), true);
  co_return;
}