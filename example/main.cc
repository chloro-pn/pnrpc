#include "pnrpc/rpc_define.h"
#include "pnrpc/rpc_server.h"
#include "pnrpc/util.h"
#include "pnrpc/net_server.h"
#include <cstring>
#include <iostream>


int main() {
  REGISTER_RPC(desc, 0x00)
  REGISTER_RPC(num_add, 0x01)

  RPCdescSTUB client;
  auto request = std::make_unique<RPC_add_request_type>();
  request->str = "hello world";
  request->age = 25;
  request->man = 0;
  client.set_request(std::move(request));
  auto msg = client.create_request_message();
  //
  // ... network
  // 
  const char* ptr = &msg[0];
  auto pcode = RpcServer::Instance().ParsePcode(ptr, msg.size());
  auto processor = RpcServer::Instance().GetProcessor(pcode);
  if (processor == nullptr) {
    std::cerr << "invalid rpc request, not find " << pcode << std::endl;
    return -1;
  }
  // parse request message
  processor->create_request_from_raw_bytes(ptr, msg.size() - sizeof(uint32_t));
  processor->process();
  auto ret = processor->create_response_to_raw_btes();
  ptr = &ret[0];
  // 
  // ... network
  //
  const std::string& result = client.create_response(ptr, ret.size());
  std::cout << result << std::endl;

  RPCnum_addSTUB c2;
  c2.set_request(std::make_unique<uint32_t>(2));
  msg = c2.create_request_message();
  ptr = &msg[0];
  pcode = RpcServer::Instance().ParsePcode(ptr, msg.size());
  processor = RpcServer::Instance().GetProcessor(pcode);
  processor->create_request_from_raw_bytes(ptr, msg.size() - sizeof(uint32_t));
  processor->process();
  ret = processor->create_response_to_raw_btes();
  uint32_t result2 = c2.create_response(&ret[0], ret.size());
  std::cout << result2 << std::endl;

  NetServer ns("127.0.0.1", 44444);
  ns.run();
  return 0;
} 