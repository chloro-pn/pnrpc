
#include "download.h"

pnrpc::net::awaitable<void> RPCDownload::process() {
  auto filename = co_await get_request_arg();
  co_await set_response_arg("hello", false);
  co_await set_response_arg("world", true);
  co_return;
}