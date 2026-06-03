load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "aarch64_as",
    srcs = glob([
        "src/arch/aarch64/**/*.S",
        "src/arch/aarch64/**/*.h",
    ]),
    hdrs = glob(["include/**/*.h"]) ,
    copts = [
        "-D__ASSEMBLY__",
        "-Wall",
        "-g",
    ],
    deps = [

    ],
    includes = ["include/", "src/arch/aarch64/include/"],
    alwayslink = True,
)

cc_library(
    name = "riscv64_as",
    srcs = glob([
        "src/arch/riscv64/**/*.S",
        "src/arch/riscv64/**/*.h",
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
    includes = ["include/", "src/arch/riscv64/include/"],
    alwayslink = True,
)

cc_library(
    name = "common_headers",
    srcs = glob(["include/**/*.h"]),
    includes = ["include/"],
)

cc_library(
    name = "start_head",
    srcs = select({
        "@platforms//cpu:aarch64": glob(
            [
                "src/arch/aarch64/**/*.cc",
                "src/arch/aarch64/**/*.h",
            ],
        ),
        "@platforms//cpu:riscv64": glob(
            [
                "src/arch/riscv64/**/*.cc",
                "src/arch/riscv64/**/*.h",
            ],
            exclude = ["src/arch/riscv64/sched_riscv.cc"],
        ),
    }),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
        "-g",
        # "-Werror",
        # "-ffreestanding",
        # "-march=armv8-a",
        # "-fpic",

        "-fno-stack-protector",
        # "-fno-builtin",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-threadsafe-statics",
        ] + select({
            "@platforms//cpu:riscv64":["-mcmodel=medany", "-march=rv64imac_zicsr", "-mabi=lp64"],
            "//conditions:default": [],
        }),
    includes = ["include/"],
    deps = select({
        "@platforms//cpu:aarch64": [
            ":aarch64_as",
            "//src/drivers:gic",
            "//src/drivers:pl011",
            ],
        "@platforms//cpu:riscv64": [
            ":riscv64_as"
            ],
    }) + [

        "@libfdt",
    ],
    alwayslink = True,
)

cc_library(
    name = "utils",
    srcs = glob(["src/utils/**/*.c", "src/utils/**/*.cc"]),
    copts = [
        "-Wall",
        "-fno-exceptions",
    ]+ select({
        "@platforms//cpu:riscv64":["-mcmodel=medany", "-march=rv64imac_zicsr", "-mabi=lp64"],
        "//conditions:default": []
    }),
    linkopts = [
        "-lc",  # need newlib here
        "-lgcc",
    ],
    hdrs = glob(["include/**/*.h"]),
    deps = [
        "//src/drivers:pl011",
    ],
    includes = ["include/"],
    alwayslink = True,
)

cc_library(
    name = "cxx-runtime",
    srcs = glob(["src/cxx_core/**/*.cc"]),
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-threadsafe-statics",
        "-O0",
        "-g",
    ] + select({
        "@platforms//cpu:riscv64": ["-mcmodel=medany", "-march=rv64imac_zicsr", "-mabi=lp64"],
        "//conditions:default": [],
    }),
    includes = ["include/"],
    deps = [":start_head"],
    alwayslink = True,
)

cc_library(
    name = "hyper-core",
    srcs = glob(
        ["src/core/**/*.cc",
        "src/core/**/*.h"],
        exclude = [
            "src/core/emul_gic.cc",
            "src/core/emul_gicv3.cc",
            "src/core/emul_uart.cc",
            "src/core/emul_dev.cc",
            "src/core/emul_psci.cc",
            "src/core/sched.cc",
        ],
    ) + select({
        "@platforms//cpu:aarch64": [
            "src/core/emul_gic.cc",
            "src/core/emul_gicv3.cc",
            "src/core/emul_uart.cc",
            "src/core/emul_dev.cc",
            "src/core/emul_psci.cc",
            "src/core/sched.cc",
        ],
        "@platforms//cpu:riscv64": [
            "src/core/emul_dev.cc",
            "src/arch/riscv64/sched_riscv.cc",
        ],
    }) + ["src/main.cc"],
    hdrs = glob(["include/**/*.h"]),
    copts = [
        "-Wall",
        "-ffreestanding",
        "-fno-stack-protector",
        "-fno-builtin",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-threadsafe-statics",
        "-O0",
        "-g",
    ] + select({
        "@platforms//cpu:riscv64": ["-mcmodel=medany", "-march=rv64imac_zicsr", "-mabi=lp64"],
        "//conditions:default": [],
    }),
    includes = [
        "include/",
    ],
    deps = [
        ":start_head",
        ":cxx-runtime",
    ],
    alwayslink = True,
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
        "include/config.h",
        ":linker_script",
    ],
    outs = [":linker.lds"],
    cmd = "gcc -E -x c $(location :linker_script) -Iinclude | grep -v \"\\#\" > $@",
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
    ] + select({
        "@platforms//cpu:aarch64": [
            "-Wl,--no-warn-rwx-segments",
        ],
        "@platforms//cpu:riscv64": [
            "-march=rv64imac_zicsr",
            "-mabi=lp64",
            "-Wl,-m,elf64lriscv",
        ],
        "//conditions:default": [],
    }),
    deps = [
        ":start_head",
        # "aarch64_as",
        ":hyper-core",
        ":utils",
        ":linker.lds",
    ],
)

genrule(
    name = "bin",
    srcs = [":hyper-elf"],
    outs = ["core.bin"],
    cmd = select({
        "@platforms//cpu:riscv64": "riscv64-linux-gnu-objcopy -O binary $< $@",
        "//conditions:default": "$(OBJCOPY) -O binary $(location :hyper-elf) $@",
    }),
    toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
)

genrule(
    name = "hyper-dtb",
    srcs = ["hyper.dts"],
    outs = ["hyper.dtb"],
    cmd = "dtc -O dtb $(location hyper.dts) > $@",
)

filegroup(
    name = "hyper",
    srcs = [
        ":bin",
        ":hyper-dtb",
        ":hyper-elf",
        ":_copy_output",
    ] +  select({
        "@platforms//cpu:aarch64": [
            "//test/arch/aarch64:guest-bin",
        ],
        "@platforms//cpu:riscv64": [
            "//test/arch/riscv64:guest-bin",
        ],
    }),
)

genrule(
    name = "_copy_output",
    srcs = [
        ":bin",
        ":hyper-dtb",
        ":hyper-elf",
    ],
    outs = ["_copy_output.stamp"],
    tags = ["no-sandbox", "no-remote"],
    stamp = True,
    cmd = """
        WS=$$(grep STABLE_WORKSPACE bazel-out/stable-status.txt | awk '{print $$2}')
        mkdir -p "$$WS/output"
        cp $(location :bin) "$$WS/output/"
        cp $(location :hyper-dtb) "$$WS/output/"
        cp $(location :hyper-elf) "$$WS/output/"
        touch $@
    """,
)

platform(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:none",
        "@platforms//cpu:aarch64",
    ],
)

platform(
    name = "linux_riscv64",
    constraint_values = [
        "@platforms//os:none",
        "@platforms//cpu:riscv64",
    ],
)
