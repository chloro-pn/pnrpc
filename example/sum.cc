#include "sum.h"
#include "pnrpc/token_bucket.h"

asio::awaitable<void> RPCSum::process() {
  auto request = (co_await get_request_arg()).value();
  uint32_t resp = 0;
  for(const auto& each : request.nums) {
    resp += each;
  }
  co_await set_response_arg(resp, true);
  co_return;
}

bool RPCSum::restrictor() {
  // 设置令牌桶的令牌入桶速率为100/s，容量为100
  static TokenBucket tb(10000, 100000);
  return tb.consume(1);
}