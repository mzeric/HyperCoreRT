package(default_visibility = ["//visibility:public"])

cc_library(
    name = "pl011",
    srcs = glob([
        "src/drivers/pl011/*.c",
        "src/drivers/pl011/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
    ],
    includes = ["include/"],
    alwayslink = True,
)

cc_library(
    name = "gic",
    srcs = glob([
        "src/drivers/gic/*.c",
        "src/drivers/gic/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
    ],
    includes = ["include/"],
    alwayslink = True,
)