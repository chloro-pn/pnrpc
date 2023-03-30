
#include "download.h"

pnrpc::net::awaitable<void> RPCDownload::process() {
  auto filename = co_await get_request_arg();
  for(int i = 0; i < 10; ++i) {
    std::string response_pkg(200, 'a');
    co_await set_response_arg(response_pkg, i == 9);
  }
  co_return;
}

// 设置写回客户端的速率不超过1024字节/秒
size_t RPCDownload::get_response_current_limiting() {
  return 1024;
}