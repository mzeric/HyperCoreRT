#!/usr/bin/env bash

#!/usr/bin/env bash

elf_file=$1
bin_file=$1
fvp_dir=/opt/arm/developmentstudio-2023.1/bin/
fvp_dir=/home/xtx/Base_RevC_AEMvA_pkg/models/Linux64_GCC-9.3
fvp_model=FVP_Base_Cortex-A78
fvp_model=FVP_Base_Cortex-A78
fvp_model=FVP_Base_AEMvA
fvp_model=FVP_Base_RevC-2xAEMvA
fvp=${fvp_dir}/${fvp_model}

function run_elf() {
${fvp} -f fvp_cfg.txt \
	-a cluster0.cpu*=./bazel-bin/hyper-elf
}
function run_2() {
${fvp} \
    -C pctl.startup=0.0.0.0 \
	-C bp.secure_memory=false                \
	-C bp.pl011_uart0.uart_enable=1          \
	-C bp.pl011_uart0.out_file=- \
	-C bp.pl011_uart0.unbuffered_output=1 \
	-C bp.pl011_uart0.untimed_fifos=1 \
	-C gic_distributor.print-memory-map=0 \
	-C cluster0.NUM_CORES=1 \
	--data cluster0.cpu0=/home/xtx/fvp-base-revc.dtb@0x83000000 \
    -C bp.secureflashloader.fname=/home/xtx/work/fvp_docker_workspace/output/aemfvp-a/aemfvp-a/tf-bl1.bin \
  	--data cluster0.cpu0=${bin_file}@0x88000000
	# -C bp.flashloader0.fname=./bazel-bin/core.bin
    # -C bp.flashloader0.fname=/home/xtx/work/fvp_docker_workspace/output/aemfvp-a/aemfvp-a/fip-uboot.bin \
	#-C cluster0.NUM_CORES=0x1                \
		# -C gic_distributor.wakeup-on-reset=1 \
	#--data cluster0.cpu*=a.bin@0x80000000
	#-C gic_distributor.wakeup-on-reset=1 \
	#--data cluster0.cpu*=jump_to_8000_0000.bin@0x0 \
	#--start 0x80000000 --data cluster0.cpu0=${bin_file}@0x80000000

}

function run_3() {
	${fvp} \
-C bp.flashloader0.fname=/home/sk/arm-TF-A-RME/build/fvp/debug/fip.bin \
-C bp.secureflashloader.fname=/home/sk/arm-TF-A-RME/build/fvp/debug/bl1.bin \
-C bp.refcounter.non_arch_start_at_default=1 \
-C bp.refcounter.use_real_time=0 \
-C bp.ve_sysregs.exit_on_shutdown=1 \
-C cache_state_modelled=1 \
-C cluster0.NUM_CORES=4 \
-C cluster0.PA_SIZE=48 \
-C cluster0.ecv_support_level=2 \
-C cluster0.gicv3.cpuintf-mmap-access-level=2 \
-C cluster0.gicv3.without-DS-support=1 \
-C cluster0.gicv4.mask-virtual-interrupt=1 \
-C cluster0.has_arm_v8-6=1 \
-C cluster0.has_branch_target_exception=1 \
-C cluster0.rme_support_level=1 \
-C cluster0.has_rndr=1 \
-C cluster0.has_amu=1 \
-C cluster0.has_v8_7_pmu_extension=2 \
-C cluster0.max_32bit_el=-1 \
-C cluster0.restriction_on_speculative_execution=2 \
-C cluster0.restriction_on_speculative_execution_aarch32=2 \
-C cluster1.NUM_CORES=4 \
-C cluster1.PA_SIZE=48 \
-C cluster1.ecv_support_level=2 \
-C cluster1.gicv3.cpuintf-mmap-access-level=2 \
-C cluster1.gicv3.without-DS-support=1 \
-C cluster1.gicv4.mask-virtual-interrupt=1 \
-C cluster1.has_arm_v8-6=1 \
-C cluster1.has_branch_target_exception=1 \
-C cluster1.rme_support_level=1 \
-C cluster1.has_rndr=1 \
-C cluster1.has_amu=1 \
-C cluster1.has_v8_7_pmu_extension=2 \
-C cluster1.max_32bit_el=-1 \
-C cluster1.restriction_on_speculative_execution=2 \
-C cluster1.restriction_on_speculative_execution_aarch32=2 \
-C pci.pci_smmuv3.mmu.SMMU_AIDR=2 \
-C pci.pci_smmuv3.mmu.SMMU_IDR0=0x0046123B \
-C pci.pci_smmuv3.mmu.SMMU_IDR1=0x00600002 \
-C pci.pci_smmuv3.mmu.SMMU_IDR3=0x1714 \
-C pci.pci_smmuv3.mmu.SMMU_IDR5=0xFFFF0475 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR1=0xA0000002 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR2=0 \
-C pci.pci_smmuv3.mmu.SMMU_S_IDR3=0 \
-C bp.pl011_uart0.out_file=uart0.log \
-C bp.pl011_uart1.out_file=uart1.log \
-C bp.pl011_uart2.out_file=uart2.log \
-C pctl.startup=0.0.0.0 \
-Q 1000 \
"$@"
}

run_elf
