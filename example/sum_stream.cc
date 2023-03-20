#include "sum_stream.h"

asio::awaitable<void> RPCSumStream::process() {
  std::unique_ptr<uint32_t> request;
  std::unique_ptr<uint32_t> ret(new uint32_t(0));
  while(request = co_await get_request_arg()) {
    *ret += *request;
  }
  co_await set_response_arg(std::move(ret), true);
}