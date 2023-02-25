#include <string>
#include "pnrpc/rpc_define.h"

int RPCdesc::process() {
  response.reset(new std::string());
  *response = request->str + ", your age is " + std::to_string(request->age) + ", and you " + (request->man == 1 ? "are " : "are not ") + "a man";
  return 0;
}

int RPCnum_add::process() {
  response.reset(new uint32_t);
  *response = *request * 10;
  return 0;
}