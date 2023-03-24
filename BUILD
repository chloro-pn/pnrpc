package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pnrpc",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/*.cc"]),
  includes = ["include"],
  deps = [
    "@bridge//:bridge",
    "@asio//:asio",
    "@spdlog//:spdlog",
    "@token_bucket//:token_bucket",
  ]
)

cc_binary(
  name = "example",
  srcs = glob(["example/*.cc", "example/*.h"]),
  deps = [
    ":pnrpc",
  ],
  includes = ["example"]
)

cc_test(
  name = "test",
  srcs = glob(["test/*.cc"]),
  deps = [
    "@bridge//:bridge",
    "@googletest//:gtest",
    "@googletest//:gtest_main",
  ]
)