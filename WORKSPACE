load("//tools/workspace:default.bzl", "add_default_repositories")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

add_default_repositories()

load("@rpi_bazel//tools/workspace:default.bzl", rpi_bazel_add = "add_default_repositories")

rpi_bazel_add()


http_archive(
    name = "googletest",
    url = "https://github.com/google/googletest/archive/release-1.10.0.tar.gz",
    sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    strip_prefix = "googletest-release-1.10.0",
)
