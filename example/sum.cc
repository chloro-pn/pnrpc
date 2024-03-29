#include "sum.h"

#include "pnrpc/token_bucket.h"

pnrpc::net::awaitable<void> RPCSum::process() {
  auto request = (co_await get_request_arg()).value();
  uint32_t resp = 0;
  for (const auto& each : request.nums) {
    resp += each;
  }
  co_await set_response_arg(resp, true);
  co_return;
}

bool RPCSum::restrictor(void* pkg) {
  const RpcSumRequestT& req = cast_to_request_pkg(pkg);
  // 设置令牌桶的令牌入桶速率为100/s，容量为100
  static TokenBucket tb(10000, 100000);
  return tb.consume(1);
}