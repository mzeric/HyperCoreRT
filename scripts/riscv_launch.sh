#!/usr/bin/env bash

QEMU=qemu-system-aarch64
QEMU=../qemu/build/qemu-system-riscv64
FW=
${QEMU} \
   -cpu rv64,h=true -M virt -m 512M \
   -nographic  \
   -monitor telnet:127.0.0.1:4444,server,nowait \
   -device loader,file=./bazel-bin/test/arch/riscv64/guest.bin,force-raw=on,addr=0x90080000 \
   -kernel ./bazel-bin/hyper-elf
   # -bios ./fw_jump.bin \
  #-device loader,file=./bazel-bin/hyper.dtb,force-raw=on,addr=0x40001000 \
   #-kernel ./bazel-bin/hyper-elf \
   #-dtb ./bazel-bin/hyper.dtb
  #-bios ./bazel-bin/core.bin
