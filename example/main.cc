#include "pnrpc/rpc_define.h"
#include "pnrpc/rpc_server.h"
#include "pnrpc/util.h"
#include "pnrpc/net_server.h"
#include "asio.hpp"
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

using namespace pnrpc;

int main() {
  REGISTER_RPC(Echo, 0x00)
  REGISTER_RPC(Sleep, 0x01)
  REGISTER_RPC(Desc, 0x02)
  REGISTER_RPC(async_task, 0x03)

  std::thread ctx_th([] {
    try {
      auto work = asio::make_work_guard(global_default_ctx);
      global_default_ctx.run();
      PNRPC_LOG_INFO("global_default_ctx stop.");
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("global_default_ctx thread exception : {}", e.what());
    }
  });

  NetServer ns("127.0.0.1", 44444, 4);
  std::thread th([&]() {
    ns.run();
  });
  std::this_thread::sleep_for(std::chrono::seconds(1));
  asio::io_context io;
  {
    RPCDescSTUB client(io, "127.0.0.1", 44444);
    auto request = std::make_unique<Desc_request_type>();
    request->str = "hello world";
    request->age = 25;
    request->man = 0;
    std::unique_ptr<std::string> response = std::make_unique<std::string>();
    auto ret_code = client.rpc_call(std::move(request), response);
    std::cout << ret_code << " " << *response << std::endl;

    RPCSleepSTUB c2(io, "127.0.0.1", 44444);
    auto r2 = std::make_unique<uint32_t>(2);
    std::unique_ptr<uint32_t> response2 = std::make_unique<uint32_t>();
    ret_code = c2.rpc_call(std::move(r2), response2);
    std::cout << ret_code  << " " << *response2 << std::endl;

    RPCasync_taskSTUB c3(io, "127.0.0.1", 44444);
    auto r3 = std::make_unique<std::string>("hello world.");
    std::unique_ptr<std::string> response3 = std::make_unique<std::string>();
    ret_code = c3.rpc_call(std::move(r3), response3);
    std::cout << ret_code  << " " << *response3 << std::endl;
  }
  global_default_ctx.stop();
  ctx_th.join();

  ns.stop();
  th.join();
  return 0;
}