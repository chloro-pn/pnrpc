package(default_visibility = ["//visibility:public"])

cc_library(
  name = "pnrpc",
  hdrs = glob(["include/**/*.h"]),
  srcs = glob(["src/*.cc"]),
  includes = ["include"],
  deps = [
    "@bridge//:bridge",
    "@spdlog//:spdlog",
    "@token_bucket//:token_bucket",
  ] + select(
    {
      "//bazel_config:pnrpc_use_boost_asio" : ["@boost//:asio"],
      "//conditions:default" : ["@asio//:asio"],
    }
  ),
  defines = select(
    {
      "//bazel_config:pnrpc_use_boost_asio" : ["PNRPC_USE_BOOST"],
      "//conditions:default" : [],
    }
  ),
)

cc_binary(
  name = "example",
  srcs = glob(["example/*.cc", "example/*.h"]),
  deps = [
    ":pnrpc",
    "@mysql//:mysql",
  ],
  includes = ["example"]
)

cc_binary(
  name = "redis_client",
  srcs = glob(["example/redis/*.cc", "example/redis/*.h"]),
  deps = [
    "@redis//:redis",
  ],
  includes = ["example/redis"],
)

cc_test(
  name = "test",
  srcs = glob(["test/*.cc", "test/*.h"]),
  includes = ["test"],
  deps = [
    ":pnrpc",
    "@mysql//:mysql",
    "@bridge//:bridge",
    "@googletest//:gtest",
    "@googletest//:gtest_main",
  ]
)