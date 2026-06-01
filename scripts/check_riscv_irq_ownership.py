#!/usr/bin/env python3

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text()


def assert_not_contains(text: str, needle: str, message: str) -> None:
    if needle in text:
        raise AssertionError(message)


def assert_contains(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def main() -> None:
    vcpu_h = read("src/arch/riscv64/include/vcpu.h")
    vcpu_cc = read("src/arch/riscv64/vcpu.cc")
    manager_cc = read("src/arch/riscv64/riscv_virt_irq_manager.cc")
    ipi_cc = read("src/arch/riscv64/ipi.cc")

    assert_not_contains(vcpu_h, "uint64_t hvip;", "cpu_arch must not store HVIP as durable vCPU state")
    assert_contains(vcpu_h, "virt_irq_lock", "vCPU virtual IRQ state must have its own lock")

    assert_not_contains(vcpu_cc, "csrr(CSR_HVIP)", "vcpu_context_save must not snapshot CSR_HVIP")
    assert_not_contains(vcpu_cc, "vcpu->carch.hvip", "vcpu context code must not restore carch.hvip")
    assert_contains(vcpu_cc, "csrw(CSR_HVIP, 0)", "context switch must clear stale HVIP hardware projection")

    assert_not_contains(manager_cc, "carch.hvip", "RiscvVirtIrqManager must use virt_irq_pending as the only durable state")
    assert_contains(manager_cc, "arch_spin_lock_irqsave", "RiscvVirtIrqManager must serialize virtual IRQ state updates")

    assert_not_contains(ipi_cc, "while (g_ipi_complete[target_cpu][ipi_vec] == before)\n        arch_cpu_relax();",
                        "sync IPI wait must be bounded")
    assert_contains(ipi_cc, "RISCV_IPI_SYNC_TIMEOUT", "sync IPI needs an explicit timeout budget")
    assert_contains(ipi_cc, "return -1;", "sync IPI timeout path must return failure")


if __name__ == "__main__":
    main()
