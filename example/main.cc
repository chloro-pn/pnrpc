#include "pnrpc/net_server.h"

#include "echo.h"
#include "sum.h"
#include "rpc_sleep.h"
#include "async.h"

#include <iostream>

using namespace pnrpc;

int main() {
  REGISTER_RPC(Sum, 0x00)
  REGISTER_RPC(Echo, 0x01)
  REGISTER_RPC(Sleep, 0x02)
  REGISTER_RPC(Async, 0x03)

  NetServer ns("127.0.0.1", 44444);
  std::thread th([&]() {
    ns.run();
  });

  std::thread ctx_th([] {
    try {
      PNRPC_LOG_INFO("global_default_ctx's address is {}", static_cast<void*>(&global_default_ctx))
      auto work = asio::make_work_guard(global_default_ctx);
      global_default_ctx.run();
      PNRPC_LOG_INFO("global_default_ctx stop.");
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("global_default_ctx thread exception : {}", e.what());
    }
  });
  // 等待服务器启动完成
  std::this_thread::sleep_for(std::chrono::seconds(1));
  asio::io_context io;

  RPCSleepSTUB client(io, "127.0.0.1", 44444);
  auto r = std::make_unique<uint32_t>(2);
  std::unique_ptr<uint32_t> response = std::make_unique<uint32_t>();
  int ret_code = client.rpc_call(std::move(r), response);
  std::cout << *response << std::endl;

  RPCEchoSTUB echo_client(io, "127.0.0.1", 44444);
  auto r2 = std::make_unique<std::string>("hello world");
  auto response2 = std::make_unique<std::string>();
  ret_code = echo_client.rpc_call(std::move(r2), response2);
  std::cout << *response2 << std::endl;

  RPCSumSTUB sum_client(io, "127.0.0.1", 44444);
  auto r3 = std::make_unique<RpcSumRequestT>();
  r3->nums.push_back(1);
  r3->nums.push_back(2);
  r3->nums.push_back(3);
  auto response3 = std::make_unique<uint32_t>();
  ret_code = sum_client.rpc_call(std::move(r3), response3);
  std::cout << *response3 << std::endl;

  RPCAsyncSTUB async_client(io, "127.0.0.1", 44444);
  auto r4 = std::make_unique<std::string>("async_args");
  auto response4 = std::make_unique<std::string>();
  ret_code = async_client.rpc_call(std::move(r4), response4);
  std::cout << *response4 << std::endl;

  global_default_ctx.stop();
  ctx_th.join();

  ns.stop();
  th.join();
  return 0;
}