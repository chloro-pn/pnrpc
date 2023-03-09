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

cc_binary(
  name = "test",
  srcs = ["test/bridge_test.cc"],
  deps = [
    "@bridge//:bridge",
  ]
)