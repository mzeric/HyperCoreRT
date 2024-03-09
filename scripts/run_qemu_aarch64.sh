#!/usr/bin/env bash

QEMU=qemu-system-aarch64
QEMU=../qemu/build/qemu-system-aarch64
${QEMU} \
   -M virt,virtualization=true,type=virt,gic-version=3,iommu="smmuv3",secure=on \
   -cpu cortex-a57  -nographic -smp 1 -m 6000 \
   -no-reboot \
   -monitor telnet:127.0.0.1:4444,server,nowait\
   -kernel ./bazel-bin/hyper-elf
  #-device loader,file=./bazel-bin/hyper.dtb,force-raw=on,addr=0x40001000 \
   #-kernel ./bazel-bin/hyper-elf \
   #-dtb ./bazel-bin/hyper.dtb
  #-bios ./bazel-bin/core.bin
