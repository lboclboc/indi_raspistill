load("//tools/workspace/rpi_bazel:repository.bzl", "rpi_bazel_repository")

def add_default_repositories(excludes = []):
    if "rpi_bazel" not in excludes:
        rpi_bazel_repository(name = "rpi_bazel")
