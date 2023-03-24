#include "bridge/adaptor.h"
#include "bridge/object.h"

#include "gtest/gtest.h"

TEST(bridge, all) {
  bridge::BridgePool bp;
  auto root = bp.map();
  root->Insert("hello", bp.data("world"));
  auto buf = bridge::Serialize(std::move(root), bp);

  auto r2 = bridge::Parse(buf, bp);
  bridge::ObjectWrapper ow(r2.get());
  EXPECT_EQ(ow["hello"].Get<std::string>().value(), "world");
}