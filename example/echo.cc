#include "echo.h"

asio::awaitable<void> RPCEcho::process() {
  auto request = co_await get_request_arg();
  co_await set_response_arg(request.value(), true);
  co_return;
}