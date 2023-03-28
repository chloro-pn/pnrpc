#include "sum_stream.h"

pnrpc::net::awaitable<void> RPCSumStream::process() {
  std::optional<uint32_t> request;
  uint32_t ret = 0;
  while(true) {
    auto requeset = co_await get_request_arg();
    if (requeset.has_value() == false) {
      break;
    }
    ret += requeset.value();
  }
  co_await set_response_arg(ret, true);
}