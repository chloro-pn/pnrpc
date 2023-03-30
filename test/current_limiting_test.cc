#include "pnrpc/current_limiting.h"

#include "gtest/gtest.h"
#include <thread>
#include <chrono>

TEST(current_limiting, all) {
  pnrpc::CurrentLimiting cl;
  EXPECT_EQ(cl.calcualte_sleep_time(100), 0);
  // 更新限流20字节/秒，如果当前写入了100字节，那么需要sleep 5秒
  cl.update_up_water_level(20);
  EXPECT_EQ(cl.calcualte_sleep_time(100), 5);
  std::this_thread::sleep_for(std::chrono::seconds(3));
  // 在3秒之后再计算sleep时间，需要sleep 3秒
  EXPECT_EQ(cl.calcualte_sleep_time(100), 2);
  cl.update_up_water_level(200);
  EXPECT_EQ(cl.calcualte_sleep_time(100), 0);
}