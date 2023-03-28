#include "echo.h"

pnrpc::net::awaitable<void> RPCEcho::process() {
  auto request = co_await get_request_arg();
  assert(request.has_value() == true);
  co_await set_response_arg(request.value(), true);
  co_return;
}