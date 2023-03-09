#include "sum.h"
#include "pnrpc/token_bucket.h"


asio::awaitable<void> RPCSum::process() {
  response = std::make_unique<uint32_t>(0.0);
  for(const auto& each : request->nums) {
    *response += each;
  }
  co_return;
}

bool RPCSum::restrictor() {
  // 设置令牌桶的令牌入桶速率为100/s，容量为100
  static TokenBucket tb(10000, 100000);
  return tb.consume(1);
}