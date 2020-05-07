config_setting(
    name = "pi_build",
    values = {"cpu": "armeabihf"},
)

cc_library(
    name = "indi_mmal_lib",
    srcs = glob(["src/*.cpp", "src/*.h",]),
    includes = ["include"],
    linkstatic = True,
)

cc_binary(
    name = "indi_mmal",

    includes = ["include"] + select({
            ":pi_build": ["@org_llvm_libcxx//:libcxx", "include/rpi"],
            "//conditions:default": ["include/x86",],
        }),

    deps = ["//:indi_mmal_lib"] + select({
            ":pi_build": ["@org_llvm_libcxx//:libcxx"],
            "//conditions:default": [],
        }),


    linkopts = select({
            ":pi_build": [],
            "//conditions:default": ['-lQt5UiTools', '-lQt5Widgets', '-lQt5Gui', '-lQt5Core', '-lncurses', '-lpthread'],
        }),

    linkstatic = True,

    features = select({
            ":pi_build": ["fully_static_link"],
            "//conditions:default": [],
        }),
)


cc_test(
    name = "TestAccelStepper",
    srcs = ["test/TestAccelStepper.cpp", "test/MockHardware.h", "test/TimingHardware.h"],
    deps = [
        "@googletest//:gtest",
        "//:libastropi",
    ],
)
