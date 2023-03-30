#include "pnrpc/net_server.h"

#include "echo.h"
#include "sum.h"
#include "rpc_sleep.h"
#include "async.h"
#include "sum_stream.h"
#include "download.h"
#include "mysql_request.h"
#include <iostream>
#include <cassert>
#include <ctime>


using namespace pnrpc;

int main() {
  REGISTER_RPC(Sum)
  REGISTER_RPC(Echo)
  REGISTER_RPC(Sleep)
  REGISTER_RPC(Async)
  REGISTER_RPC(SumStream)
  REGISTER_RPC(Download)
  REGISTER_RPC(MysqlRequest)

  NetServer ns("127.0.0.1", 44444, 4);
  std::thread th([&]() {
    ns.run();
  });

  std::thread ctx_th([] {
    try {
      global_default_ctx.reset(new pnrpc::net::io_context());
      PNRPC_LOG_INFO("global_default_ctx's address is {}", static_cast<void*>(global_default_ctx.get()))
      auto work = pnrpc::net::make_work_guard(*global_default_ctx);
      global_default_ctx->run();
      PNRPC_LOG_INFO("global_default_ctx stop.");
    } catch (std::exception& e) {
      PNRPC_LOG_ERROR("global_default_ctx thread exception : {}", e.what());
    }
  });
  // 等待服务器启动完成
  std::this_thread::sleep_for(std::chrono::seconds(1));
  pnrpc::net::io_context io;

  RPCEchoSTUB echo_client(io, "127.0.0.1", 44444);
  echo_client.connect();
  std::string resp;
  uint32_t ret_code = echo_client.rpc_call("helloworld", resp);
  assert(ret_code == RPC_OK);
  assert(resp == "helloworld");
  
  pnrpc::net::co_spawn(io, [&io]() -> pnrpc::net::awaitable<void> {
    RPCSumStreamSTUB sum_client(io, "127.0.0.1", 44444);
    co_await sum_client.async_connect();
    for(uint32_t x = 0; x < 3; ++x) {
      co_await sum_client.send_request(x);
    }
    co_await sum_client.send_request(3, true);
    uint32_t resp = 0;
    int ret_code = co_await sum_client.recv_response(resp);
    assert(resp == 0 + 1 + 2 + 3);
    assert(ret_code == RPC_OK);
  }, pnrpc::net::detached);

  pnrpc::net::co_spawn(io, [&io]() -> pnrpc::net::awaitable<void> {
    RPCDownloadSTUB client(io, "127.0.0.1", 44444);
    co_await client.async_connect();
    co_await client.send_request("test.txt");
    std::string tmp;
    std::optional<std::string> response;
    while(co_await client.recv_response(response) == RPC_OK && response.has_value()) {
      auto now = std::chrono::system_clock::now();
      auto now_time_t = std::chrono::system_clock::to_time_t(now);
      PNRPC_LOG_INFO("rpc download stub recv at {}", std::ctime(&now_time_t));
      tmp += response.value();
    }
  }, pnrpc::net::detached);
  
  pnrpc::net::co_spawn(io, [&io]() -> pnrpc::net::awaitable<void> {
    RPCMysqlRequestSTUB client(io, "127.0.0.1", 44444);
    co_await client.async_connect();
    MysqlRequestRpcT req;
    req.user_name = "pn";
    req.password = "123456";
    req.db_name = "test";
    std::string resp;
    assert(co_await client.rpc_call_coro(req, resp) == RPC_OK);
    assert(resp == "Hello World!");
  }, pnrpc::net::detached);

  io.run();
  io.stop();
  
  global_default_ctx->stop();
  ctx_th.join();
  // 注意，io_context的生命周期很重要，并不是只要注意stop顺序就好了。
  global_default_ctx.reset();
  ns.stop();
  th.join();
  return 0;
}