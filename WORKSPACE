load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository', 'new_git_repository')
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
  name = "bridge",
  remote = "https://github.com/chloro-pn/bridge",
  commit = "21e8c187dc67b73145a7bab462f0a39773325f57",
)

git_repository(
    name = "googletest",
    remote = "https://ghproxy.com/https://github.com/google/googletest",
    tag = "release-1.11.0",
)

new_git_repository(
    name = "async_simple",
    remote = "https://github.com/alibaba/async_simple",
    commit = "04885f0a59f9a848a67d43d6be2bb9ad2e32d739",
    build_file = "//third_party:async_simple.build",
)

new_git_repository(
  name = "asio",
  remote = "https://github.com/chriskohlhoff/asio",
  tag = "asio-1-24-0",
  build_file = "//third_party:asio.build",
)

new_git_repository(
    name = "spdlog",
    remote = "https://github.com/gabime/spdlog",
    tag = "v1.10.0",
    build_file = "//third_party:spdlog.build",
)

new_git_repository(
  name = "token_bucket",
  remote = "https://github.com/rigtorp/TokenBucket",
  branch = "master",
  build_file = "//third_party:token_bucket.build",
)

http_archive(
  name = "com_github_nelhage_rules_boost",

  # Replace the commit hash in both places (below) with the latest, rather than using the stale one here.
  # Even better, set up Renovate and let it do the work for you (see "Suggestion: Updates" in the README).
  url = "https://github.com/nelhage/rules_boost/archive/5729d34dcf595874f32b9f1aa1134db65fe78fda.tar.gz",
  strip_prefix = "rules_boost-5729d34dcf595874f32b9f1aa1134db65fe78fda",
  # When you first run this tool, it'll recommend a sha256 hash to put here with a message like: "DEBUG: Rule 'com_github_nelhage_rules_boost' indicated that a canonical reproducible form can be obtained by modifying arguments sha256 = ..."
)

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()