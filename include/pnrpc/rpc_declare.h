#pragma once

#include "pnrpc/rpc_client.h"
#include "pnrpc/rpc_server.h"

#define RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, rpc_type, ...)              \
  class RPC##funcname : public pnrpc::RpcProcessor<request_t, response_t, pcode, rpc_type> {  \
   public:                                                                                    \
    RPC##funcname() : pnrpc::RpcProcessor<request_t, response_t, pcode, rpc_type>() {}        \
    __VA_ARGS__                                                                               \
  };                                                                                          \
                                                                                              \
  class RPC##funcname##STUB : public pnrpc::RpcStub<request_t, response_t, pcode, rpc_type> { \
   public:                                                                                    \
    RPC##funcname##STUB(pnrpc::net::io_context& io, const std::string& ip, uint16_t port)     \
        : pnrpc::RpcStub<request_t, response_t, pcode, rpc_type>(io, ip, port) {}             \
  };

#define OVERRIDE_BIND pnrpc::net::io_context* bind_io_context(void*) override;

#define OVERRIDE_PROCESS pnrpc::net::awaitable<void> process() override;

#define OVERRIDE_RESTRICTOR bool restrictor(void*) override;

#define OVERRIDE_REQUEST_LIMIT size_t get_request_current_limiting(void*) override;

#define OVERRIDE_RESPONSE_LIMIT size_t get_response_current_limiting(void*) override;

#define RPC_DECLARE(funcname, request_t, response_t, pcode, rpc_type, ...) \
  RPC_DECLARE_INNER(funcname, request_t, response_t, pcode, rpc_type, __VA_ARGS__)

#define REGISTER_RPC(funcname)                                                                                      \
  pnrpc::RpcServer::Instance().RegisterRpc(RPC##funcname::pcode, []() -> std::unique_ptr<pnrpc::RpcProcessorBase> { \
    return std::make_unique<RPC##funcname>();                                                                       \
  });