#pragma once

#include "TokenBucket.h"

#include <cstdint>

namespace pnrpc {

class TokenBucketRestrictor {
 public:
  TokenBucketRestrictor(uint64_t rate, uint64_t size) : tb(rate, size) {

  }

  bool alloc() { return tb.consume(1); }

 private:
  TokenBucket tb;
};

}