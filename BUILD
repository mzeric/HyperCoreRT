load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "aarch64_as",
    srcs = glob([
        "src/arch/aarch64/**/*.S",
        "src/arch/aarch64/**/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-D__ASSEMBLY__",
        "-Wall",
    ],
    includes = ["include/"],
    alwayslink = True,
)

cc_library(
    name = "start_head",
    srcs = select({
        "@platforms//cpu:aarch64": glob(
            [
                "src/arch/aarch64/**/*.c",
                # "src/arch/aarch64/**/*.S",
                "src/arch/aarch64/**/*.h",
            ],
        ),
        "@platforms//cpu:riscv64": glob(
            [
                "src/arch/riscv64/**/*.c",
                "src/arch/riscv64/**/*.S",
                "src/arch/riscv64/**/*.h",
            ],
        ),
    }),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
        # "-fpic",
    ],
    includes = ["include/"],
    deps = [
        ":aarch64_as",
        "@libfdt",

    ],
    alwayslink = True,
)

cc_library(
    name = "utils",
    srcs = glob(["src/utils/**/*.c"]),
    copts = [
        "-Wall",
    ],
    linkopts = [
        "-lc",  # need newlib here
        "-lgcc",
    ],
    alwayslink = True,
)

cc_library(
    name = "hyper-core",
    srcs = glob(
        ["src/core/**/*.c",
        "src/core/**/*.h"],
    ) + ["src/main.c"],
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
    ],
    includes = [
        "include/",
    ],
)

filegroup(
    name = "linker_script",
    srcs = select({
        "@platforms//cpu:aarch64": glob(["src/arch/aarch64/*.ld*"]),
        "@platforms//cpu:riscv64": glob(["src/arch/riscv64/*.ld*"]),
    }),
)

genrule(
    name = "ld_script",
    srcs = [
        "include/autoconf.h",
        ":linker_script",
    ],
    outs = [":linker.lds"],
    cmd = "$(CC) -E -x c $(location :linker_script) -Iinclude |grep -v \"\\#\" > $@",
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)

cc_binary(
    name = "hyper-elf",

    linkopts = [
        "-nostdlib",
        "-nostartfiles",
        "-nodefaultlibs",
    ] + [
        "-Wl,-T$(location :linker.lds)",
        "-Wl,--build-id=none",
    ],
    deps = [
        ":start_head",
        ":hyper-core",
        ":utils",
        ":linker.lds",
    ],
)

genrule(
    name = "bin",
    srcs = [":hyper-elf"],
    outs = ["core.bin"],
    # tools = [":dtb"],
    cmd = "$(OBJCOPY) -O binary $(location :hyper-elf) $@",
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)

genrule(
    name = "dtb",
    srcs = ["hyper.dts"],
    outs = ["hyper.dtb"],
    cmd = "dtc -O dtb $(location hyper.dts) > $@",
)

genrule(
    name = "dtb2",
    srcs = ["hyper.dts"],
    outs = ["hyper2.dtb"],
    cmd = "dtc -O dtb $(location hyper.dts) > $@",
)

filegroup(
    name = "hyper",
    srcs = [
        ":bin",
        ":dtb",
        ":hyper-elf",
    ],
)

config_setting(
    name = "arm_cpu",
    values = {"cpu": "arm"},
)

config_setting(
    name = "x86_cpu",
    values = {"cpu": "x86_64"},
)

config_setting(
    name = "riscv_cpu",
    values = {"cpu": "riscv"},
)

platform(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

platform(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:aarch64",
    ],
)

platform(
    name = "linux_riscv64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:riscv64",
    ],
)
