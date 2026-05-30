load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "assembly",
    srcs = glob([
        "**/*.S",
        "**/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-D__ASSEMBLY__",
        "-Wall",
        "-mcmodel=medany",
        "-march=rv64imac_zicsr",
        "-mabi=lp64",
    ],
    deps = [

    ],
    includes = ["include/"],
    alwayslink = True,
)

cc_library(
    name = "guest-core",
    srcs = glob(
        ["**/*.c",
        "**/*.h"],

    ) ,
    hdrs = glob(["include/**/*.h"]) ,
    copts = [
        "-Wall",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
        "-mcmodel=medany",
        "-march=rv64imac_zicsr",
        "-mabi=lp64",
    ],
    includes = [
        "include/",
    ],
    deps = [
        ":assembly",
        "//:common_headers",
        "//:utils",
    ],
    alwayslink = True,

)

cc_binary(
    name = "guest-elf",

    linkopts = [
        "-nostdlib",
        "-nostartfiles",
        "-nodefaultlibs",
        "-march=rv64imac_zicsr",
        "-mabi=lp64",
    ] + [
        "-Wl,-T$(location :linker.lds)",
        "-Wl,--build-id=none",
        "-Wl,-m,elf64lriscv",
    ],
    deps = [
        ":guest-core",
        ":linker.lds",
    ],
)

genrule(
    name = "guest-bin",
    srcs = [":guest-elf"],
    outs = ["guest.bin"],
    cmd = "riscv64-linux-gnu-objcopy -O binary $< $@",
)