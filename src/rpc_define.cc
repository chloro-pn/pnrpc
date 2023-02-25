#include <string>
#include "pnrpc/rpc_define.h"

int RPCadd::process() {
  response.reset(new std::string());
  *response = request->str + ", marked by chloro";
  return 0;
}

int RPCnum_add::process() {
  response.reset(new uint32_t);
  *response = *request * 10;
  return 0;
}