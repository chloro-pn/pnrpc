package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pnrpc",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/*.cc"]),
  includes = ["include"],
  deps = [
    "@bridge//:bridge",
    "@asio//:asio",
  ]
)

cc_binary(
  name = "example",
  srcs = ["example/main.cc"],
  deps = [
    ":pnrpc",
  ]
)

cc_binary(
  name = "test",
  srcs = ["test/bridge_test.cc"],
  deps = [
    "@bridge//:bridge",
  ]
)