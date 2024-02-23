#!/usr/bin/env bash

QEMU=qemu-system-aarch64
QEMU=/mnt/sdb1/xtx/work/qemu/build/qemu-system-aarch64
${QEMU} \
   -M virt,virtualization=true,type=virt,gic-version=3,iommu="smmuv3" \
   -cpu max  -nographic -smp 4 -m 6000 \
   -no-reboot \
   -kernel ./bazel-bin/hyper-elf \
   -monitor telnet:127.0.0.1:4444,server,nowait\
  -device loader,file=./bazel-bin/hyper.dtb,force-raw=on,addr=0x40001000 \
   #-dtb ./bazel-bin/hyper.dtb
