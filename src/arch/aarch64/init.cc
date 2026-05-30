
#include "board_cfg_qemu_virt.h"
#include "config.h"
#include "aarch64_system.h"
#include "vmio.h"
#include "inline_asm.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "processor.h"
#include "page.h"
#include "mm.h"
#include "excep.h"
#include "fdt_helper.h"
#include "sched.h"
#include "io.h"
#include "src/drivers/gic/gicv3.h"
#include "vcpu.h"
#include "arch_barrier.h"
#include "sys_reg.h"
#include "src/drivers/gic/gic_ops.h"
#include "src/drivers/pl011/pl011.h"
#include "utils.h"
#include "emul_dev.h"
#include "emul_gic.h"
#include "hyper_config.h"
#include "smp.h"
#include "percpu.h"
#include "ipi.h"

#include <ioremap.h>

void timer_init(void);

#ifdef CONFIG_BOARD_QEMU_VIRT
extern "C" void arch_putchar(char ch) { *(volatile int *)0x09000000 = ch; }

#elif defined(CONFIG_BOARD_FVP_AEMVA)
extern "C" void arch_putchar(char ch) { *(volatile int *)0x1c090000 = ch; }
#endif

struct mmu_lpae_entry_ctrl {
    uint32_t ttbl_count;
    uint64_t* next_ttbl;
    vaddr_t ttbl_base;
};

struct mm_region {
    uint64_t virt;
    uint64_t phys;
    uint64_t size;
    uint64_t attrs;
};

static inline void cpu_mmu_clean_invalidate(vaddr_t va) {
    asm volatile("dc civac, %0\t\n"
                 "dsb sy\t\n"
                 "isb\t\n"
                 :
                 : "r"((unsigned long)va));
}

void arm_inv_cache_range(const vaddr_t base, size_t size) {
    unsigned dcache_lsize = 0;
    static unsigned int cache_info = 0;
    const char* address;
    const char* end = (const char*)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char*)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char*)end; address += dcache_lsize) {
        asm volatile("dc ivac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy; isb" : : : "memory");
}

void arm_flush_cache_range(const vaddr_t base, size_t size) {
    unsigned dcache_lsize = 0;
    static unsigned int cache_info = 0;
    const char* address;
    const char* end = (const char*)((uintptr_t)base + size);

    if (!cache_info) {
        /*  CTR_EL0 [3:0]   contains log2 of icache line size in words
         *  CTR_EL0 [19:16] contains log2 of dcache line size in words
         */
        asm volatile("mrs %0, ctr_el0" : "=r"(cache_info));
    }
    dcache_lsize = 4 << ((cache_info >> 16) & 0xF);

    address = (const char*)((uintptr_t)base & ~(uintptr_t)(dcache_lsize - 1));
    for (; address < (const char*)end; address += dcache_lsize) {
        asm volatile("dc cvac, %0" : : "r"(address) : "memory");
    }

    asm volatile("dsb sy\nisb" : : : "memory");
}

static inline void cpu_mmu_invalidate_range(vaddr_t start, vaddr_t size) {
    arm_inv_cache_range(start, start + size);
}


void init_stage1_mm(void) {}

extern void* __hyp_vectors;
static void *g_early_data_address __attribute__((unused));

void early_uart_init(void) {

    pl011_init(0);
    // hyper_printf("UART/PL011 Enabled\n");
}

int id2pa_range(int id) {
    static const uint8_t pamax_map[] = {
            [0] = 32,
            [1] = 36,
            [2] = 40,
            [3] = 42,
            [4] = 44,
            [5] = 48,
            [6] = 52,
    };
    if (id < 0 || id > 6)
        return -1;
    return pamax_map[id];
}

int get_phys_id() {
    uint64_t id0 = mrs(ID_AA64MMFR0_EL1);

    return id0 & 0xF;
}

int get_phys_bits() {
    uint64_t id0 = mrs(ID_AA64MMFR0_EL1);
    int v = id2pa_range(id0 & 0xF);
    return (v < 48) ? v : 48;
}

#define HYPER_DEFAULT_GUEST_ENTRY      0x50200000ULL
#define HYPER_DEFAULT_GUEST_DTB        0x65000000ULL
#define HYPER_DEFAULT_GUEST_MEM_BASE   0x40000000ULL
#define HYPER_DEFAULT_GUEST_MEM_SIZE   0x20000000ULL
#define HYPER_DEFAULT_GICD_BASE        0x08000000ULL
#define HYPER_DEFAULT_GICD_SIZE        0x00010000ULL
#define HYPER_DEFAULT_GICR_BASE        0x080a0000ULL
#define HYPER_DEFAULT_GICR_SIZE        0x00f60000ULL
#define HYPER_DEFAULT_GICR_STRIDE      0x00020000ULL
#define HYPER_DEFAULT_VCPU_COUNT       2U
#define HYPER_DEFAULT_HYP_TIMER_PPI    26U
#define HYPER_DEFAULT_GUEST_TIMER_PPI  27U

struct hyper_config g_hyper_config;

struct hyper_config *hyper_config(void)
{
    return &g_hyper_config;
}

static void hyper_config_set_defaults(struct hyper_config *cfg)
{
    cfg->memory.base = 0;
    cfg->memory.size = 0;

    cfg->host_gic.gicd_base = HYPER_DEFAULT_GICD_BASE;
    cfg->host_gic.gicd_size = HYPER_DEFAULT_GICD_SIZE;
    cfg->host_gic.gicr_base = HYPER_DEFAULT_GICR_BASE;
    cfg->host_gic.gicr_size = 0x00200000ULL;
    cfg->host_gic.gicr_stride = HYPER_DEFAULT_GICR_STRIDE;
    cfg->host_gic.gicr_count = cfg->host_gic.gicr_size / cfg->host_gic.gicr_stride;

    cfg->timer.hyp_timer_ppi = HYPER_DEFAULT_HYP_TIMER_PPI;
    cfg->timer.guest_virt_timer_ppi = HYPER_DEFAULT_GUEST_TIMER_PPI;

    cfg->uart.host_base = 0x09000000ULL;
    cfg->uart.host_size = 0x1000ULL;
    cfg->uart.host_irq = GIC_SPI_INTID(1U);
    cfg->uart.guest_base = 0x09000000ULL;
    cfg->uart.guest_size = 0x1000ULL;
    cfg->uart.guest_irq = GIC_SPI_INTID(1U);
    cfg->uart.enabled = 1;

    cfg->guest.entry = HYPER_DEFAULT_GUEST_ENTRY;
    cfg->guest.dtb_addr = HYPER_DEFAULT_GUEST_DTB;
    cfg->guest.memory.base = HYPER_DEFAULT_GUEST_MEM_BASE;
    cfg->guest.memory.size = HYPER_DEFAULT_GUEST_MEM_SIZE;
    cfg->guest.vcpu_count = HYPER_DEFAULT_VCPU_COUNT;
    for (u32 i = 0; i < HYPER_MAX_VCPUS; ++i)
        cfg->guest.vcpu_mpidr[i] = i;

    cfg->guest.vgic.gicd_base = HYPER_DEFAULT_GICD_BASE;
    cfg->guest.vgic.gicd_size = HYPER_DEFAULT_GICD_SIZE;
    cfg->guest.vgic.gicr_base = HYPER_DEFAULT_GICR_BASE;
    cfg->guest.vgic.gicr_size = HYPER_DEFAULT_GICR_SIZE;
    cfg->guest.vgic.gicr_stride = HYPER_DEFAULT_GICR_STRIDE;
}

static u64 fdt_read_cells(const fdt32_t *cells, int cell_count)
{
    u64 value = 0;

    for (int i = 0; i < cell_count; ++i)
        value = (value << 32) | fdt32_to_cpu(cells[i]);
    return value;
}

static int fdt_get_inherited_cell_count(void *fdt, int node, const char *name, int default_value)
{
    int len;

    while (node >= 0) {
        const fdt32_t *prop = (const fdt32_t *)fdt_getprop(fdt, node, name, &len);
        if (prop && len >= (int)sizeof(fdt32_t))
            return fdt32_to_cpu(prop[0]);
        node = fdt_parent_offset(fdt, node);
    }
    return default_value;
}

static int fdt_get_reg_entry(void *fdt, int node, int index, u64 *addr, u64 *size)
{
    int len;
    int parent = fdt_parent_offset(fdt, node);
    int na = fdt_get_inherited_cell_count(fdt, parent, "#address-cells", 2);
    int ns = fdt_get_inherited_cell_count(fdt, parent, "#size-cells", 2);
    const fdt32_t *p = (const fdt32_t *)fdt_getprop(fdt, node, "reg", &len);

    if (!p)
        return -1;

    int entry_cells = na + ns;
    int off = index * entry_cells;
    if (len < (off + entry_cells) * (int)sizeof(fdt32_t))
        return -2;

    *addr = fdt_read_cells(p + off, na);
    *size = fdt_read_cells(p + off + na, ns);
    return 0;
}

static int fdt_read_u32_prop(void *fdt, int node, const char *name, u32 *value)
{
    int len;
    const fdt32_t *prop = (const fdt32_t *)fdt_getprop(fdt, node, name, &len);

    if (!prop || len < (int)sizeof(fdt32_t))
        return -1;
    *value = fdt32_to_cpu(prop[0]);
    return 0;
}

static int fdt_read_u64_prop(void *fdt, int node, const char *name, u64 *value)
{
    int len;
    const fdt32_t *prop = (const fdt32_t *)fdt_getprop(fdt, node, name, &len);

    if (!prop || len < (int)sizeof(fdt32_t))
        return -1;
    *value = fdt_read_cells(prop, len >= 2 * (int)sizeof(fdt32_t) ? 2 : 1);
    return 0;
}

static int fdt_read_gic_interrupt(void *fdt, int node, int index, u32 *intid)
{
    int len;
    const fdt32_t *prop = (const fdt32_t *)fdt_getprop(fdt, node, "interrupts", &len);
    int off = index * 3;
    u32 type;
    u32 irq;

    if (!prop || len < (off + 3) * (int)sizeof(fdt32_t))
        return -1;

    type = fdt32_to_cpu(prop[off]);
    irq = fdt32_to_cpu(prop[off + 1]);
    if (type == 0) {
        *intid = GIC_SPI_INTID(irq);
        return 0;
    }
    if (type == 1) {
        *intid = 16U + irq;
        return 0;
    }
    return -2;
}

static int find_guest_memory_node(void *fdt, int guest_node)
{
    int mem_node = fdt_node_offset_by_prop_value(fdt, guest_node, "device_type", "memory", sizeof("memory"));

    if (mem_node >= 0)
        return mem_node;
    return fdt_node_offset_by_prop_value(fdt, guest_node, "device_memory", "memory", sizeof("memory"));
}

static int fdt_direct_child_by_compatible(void *fdt, int parent, const char *compat)
{
    int node;

    for (node = fdt_first_subnode(fdt, parent); node >= 0; node = fdt_next_subnode(fdt, node)) {
        if (!fdt_node_check_compatible(fdt, node, compat))
            return node;
    }
    return -1;
}

static void parse_host_gic_cfg(void *fdt, int root_node, struct hyper_config *cfg)
{
    u64 addr, size;
    int gic_node = fdt_node_offset_by_compatible(fdt, root_node, "arm,gic-v3");

    if (gic_node < 0)
        return;
    if (!fdt_get_reg_entry(fdt, gic_node, 0, &addr, &size)) {
        cfg->host_gic.gicd_base = addr;
        cfg->host_gic.gicd_size = size;
    }
    if (!fdt_get_reg_entry(fdt, gic_node, 1, &addr, &size)) {
        cfg->host_gic.gicr_base = addr;
        cfg->host_gic.gicr_size = size;
    }
    fdt_read_u64_prop(fdt, gic_node, "redistributor-stride", &cfg->host_gic.gicr_stride);
    if (cfg->host_gic.gicr_stride)
        cfg->host_gic.gicr_count = cfg->host_gic.gicr_size / cfg->host_gic.gicr_stride;
}

static void parse_host_uart_cfg(void *fdt, int root_node, struct hyper_config *cfg)
{
    u64 addr, size;
    u32 irq;
    int uart_node = fdt_direct_child_by_compatible(fdt, root_node, "arm,pl011");

    if (uart_node < 0)
        return;
    if (!fdt_get_reg_entry(fdt, uart_node, 0, &addr, &size)) {
        cfg->uart.host_base = addr;
        cfg->uart.host_size = size;
        cfg->uart.enabled = 1;
    }
    if (!fdt_read_gic_interrupt(fdt, uart_node, 0, &irq))
        cfg->uart.host_irq = irq;
}

static void parse_guest_uart_cfg(void *fdt, int guest_node, struct hyper_config *cfg)
{
    u64 addr, size;
    u32 irq;
    int uart_node = fdt_direct_child_by_compatible(fdt, guest_node, "arm,pl011");

    if (uart_node < 0)
        return;
    if (!fdt_get_reg_entry(fdt, uart_node, 0, &addr, &size)) {
        cfg->uart.guest_base = addr;
        cfg->uart.guest_size = size;
        cfg->uart.enabled = 1;
    }
    if (!fdt_read_gic_interrupt(fdt, uart_node, 0, &irq))
        cfg->uart.guest_irq = irq;
}

static void parse_timer_cfg(void *fdt, int root_node, int guest_node, struct hyper_config *cfg)
{
    fdt_read_u32_prop(fdt, root_node, "hyp-timer-ppi", &cfg->timer.hyp_timer_ppi);
    fdt_read_u32_prop(fdt, root_node, "guest-virt-timer-ppi", &cfg->timer.guest_virt_timer_ppi);
    if (guest_node >= 0) {
        fdt_read_u32_prop(fdt, guest_node, "hyp-timer-ppi", &cfg->timer.hyp_timer_ppi);
        fdt_read_u32_prop(fdt, guest_node, "guest-virt-timer-ppi", &cfg->timer.guest_virt_timer_ppi);
    }
}

static void parse_guest_vcpus(void *fdt, int guest_node, struct hyper_config *cfg)
{
    int len;
    u32 count;
    const fdt32_t *mpidrs;

    if (!fdt_read_u32_prop(fdt, guest_node, "vcpu-count", &count)) {
        cfg->guest.vcpu_count = (count < (u32)HYPER_MAX_VCPUS) ? count : (u32)HYPER_MAX_VCPUS;
        for (u32 i = 0; i < cfg->guest.vcpu_count; ++i)
            cfg->guest.vcpu_mpidr[i] = i;
    }

    mpidrs = (const fdt32_t *)fdt_getprop(fdt, guest_node, "vcpu-mpidrs", &len);
    if (mpidrs) {
        u32 n = ((u32)(len / (2 * sizeof(fdt32_t))) < (u32)HYPER_MAX_VCPUS) ? (u32)(len / (2 * sizeof(fdt32_t))) : (u32)HYPER_MAX_VCPUS;
        cfg->guest.vcpu_count = n;
        for (u32 i = 0; i < n; ++i)
            cfg->guest.vcpu_mpidr[i] = fdt_read_cells(mpidrs + i * 2, 2);
    }
}

static void parse_guest_vgic_cfg(void *fdt, int guest_node, struct hyper_config *cfg)
{
    u64 addr, size;
    int gic_emul_node = fdt_node_offset_by_compatible(fdt, guest_node, "hypervisor,vgic-v3");

    if (gic_emul_node < 0)
        return;
    if (!fdt_get_reg_entry(fdt, gic_emul_node, 0, &addr, &size)) {
        cfg->guest.vgic.gicd_base = addr;
        cfg->guest.vgic.gicd_size = size;
    }
    if (!fdt_get_reg_entry(fdt, gic_emul_node, 1, &addr, &size)) {
        cfg->guest.vgic.gicr_base = addr;
        cfg->guest.vgic.gicr_size = size;
    }
    fdt_read_u64_prop(fdt, gic_emul_node, "redistributor-stride", &cfg->guest.vgic.gicr_stride);
}

static bool guest_dtb_mpidr_configured(struct hyper_config *cfg, u64 mpidr)
{
    mpidr &= 0xff00ffffffULL;
    for (u32 i = 0; i < cfg->guest.vcpu_count; ++i) {
        if ((cfg->guest.vcpu_mpidr[i] & 0xff00ffffffULL) == mpidr)
            return true;
    }
    return false;
}

static void validate_guest_dtb_config(struct hyper_config *cfg)
{
    void *fdt = (void *)cfg->guest.dtb_addr;
    u32 cpu_count = 0;
    u64 addr, size;

    if (fdt_check_header(fdt)) {
        safe_printf("guest dtb header invalid at %lx\n", cfg->guest.dtb_addr);
        panic("invalid guest dtb");
    }

    int cpus_node = fdt_path_offset(fdt, "/cpus");
    if (cpus_node < 0) {
        safe_printf("guest dtb missing /cpus\n");
        panic("invalid guest dtb");
    }

    for (int node = fdt_first_subnode(fdt, cpus_node); node >= 0; node = fdt_next_subnode(fdt, node)) {
        const char *type = (const char *)fdt_getprop(fdt, node, "device_type", NULL);
        if (!type || strcmp(type, "cpu"))
            continue;
        if (fdt_get_reg_entry(fdt, node, 0, &addr, &size)) {
            safe_printf("guest dtb cpu node missing reg: %s\n", fdt_get_name(fdt, node, NULL));
            panic("invalid guest dtb");
        }
        if (!guest_dtb_mpidr_configured(cfg, addr)) {
            safe_printf("guest dtb cpu mpidr 0x%lx not configured\n", addr);
            panic("invalid guest dtb");
        }
        cpu_count++;
    }

    if (cpu_count != cfg->guest.vcpu_count) {
        safe_printf("guest dtb cpu count %d != hyper config %d\n", cpu_count, cfg->guest.vcpu_count);
        panic("invalid guest dtb");
    }

    int gic_node = fdt_node_offset_by_compatible(fdt, -1, "arm,gic-v3");
    if (gic_node < 0) {
        safe_printf("guest dtb missing arm,gic-v3\n");
        panic("invalid guest dtb");
    }
    addr = 0;
    size = 0;
    if (fdt_get_reg_entry(fdt, gic_node, 0, &addr, &size) ||
        addr != cfg->guest.vgic.gicd_base || size != cfg->guest.vgic.gicd_size) {
        safe_printf("guest dtb GICD mismatch: <%lx,%lx> expected <%lx,%lx>\n",
                    addr, size, cfg->guest.vgic.gicd_base, cfg->guest.vgic.gicd_size);
        panic("invalid guest dtb");
    }
    addr = 0;
    size = 0;
    if (fdt_get_reg_entry(fdt, gic_node, 1, &addr, &size) ||
        addr != cfg->guest.vgic.gicr_base || size != cfg->guest.vgic.gicr_size) {
        safe_printf("guest dtb GICR mismatch: <%lx,%lx> expected <%lx,%lx>\n",
                    addr, size, cfg->guest.vgic.gicr_base, cfg->guest.vgic.gicr_size);
        panic("invalid guest dtb");
    }

    if (cfg->uart.enabled) {
        u32 irq = 0;
        int uart_node = fdt_node_offset_by_compatible(fdt, -1, "arm,pl011");
        if (uart_node < 0) {
            safe_printf("guest dtb missing arm,pl011\n");
            panic("invalid guest dtb");
        }
        addr = 0;
        size = 0;
        if (fdt_get_reg_entry(fdt, uart_node, 0, &addr, &size) ||
            addr != cfg->uart.guest_base || size != cfg->uart.guest_size) {
            safe_printf("guest dtb PL011 mismatch: <%lx,%lx> expected <%lx,%lx>\n",
                        addr, size, cfg->uart.guest_base, cfg->uart.guest_size);
            panic("invalid guest dtb");
        }
        if (fdt_read_gic_interrupt(fdt, uart_node, 0, &irq) || irq != cfg->uart.guest_irq) {
            safe_printf("guest dtb PL011 irq mismatch: %d expected %d\n", irq, cfg->uart.guest_irq);
            panic("invalid guest dtb");
        }
    }

    safe_printf("guest dtb topology validated: cpus=%d vgic=<%lx,%lx>\n",
                cpu_count, cfg->guest.vgic.gicd_base, cfg->guest.vgic.gicr_base);
}

int parse_guest_cfg(void *dtb_base, int guest_node, struct hyper_config *cfg)
{
    u64 mem_size;
    u64 mem_addr;

    if (guest_node < 0) {
        safe_printf("invalid guest node: %d\n", guest_node);
        return -1;
    }

    fdt_read_u64_prop(dtb_base, guest_node, "guest-entry", &cfg->guest.entry);
    fdt_read_u64_prop(dtb_base, guest_node, "guest-dtb", &cfg->guest.dtb_addr);
    parse_guest_vcpus(dtb_base, guest_node, cfg);
    parse_guest_vgic_cfg(dtb_base, guest_node, cfg);
    parse_guest_uart_cfg(dtb_base, guest_node, cfg);

    int mem_node = find_guest_memory_node(dtb_base, guest_node);
    if (mem_node < 0) {
        safe_printf("no memory cfg for guest:%s\n", fdt_get_name(dtb_base, guest_node, NULL));
        return -1;
    }

    if (fdt_get_reg_info(dtb_base, mem_node, &mem_addr, &mem_size) < 0)
        return -1;

    cfg->guest.memory.base = mem_addr;
    cfg->guest.memory.size = mem_size;
    safe_printf("get guest:%s memory:%s <start:%lx, size:%lx>\n",
                fdt_get_name(dtb_base, guest_node, NULL),
                fdt_get_name(dtb_base, mem_node, NULL),
                mem_addr,
                mem_size);
    return 0;
}

int load_dtb(uintptr_t dtb_base, struct hyper_config *cfg)
{
    // char* fdt = (uint64_t*)0x40000000;
#if 0
    char *fdt = (char *)ioremap_page(dtb_base, MT_NORMAL);
#else
    char *fdt = (char *)dtb_base;
#endif
    u64 mem_addr;
    u64 mem_size;
    int root_node = -1;

    hyper_config_set_defaults(cfg);
    safe_printf("load dtb from %p\n", dtb_base);

    root_node = fdt_node_offset_by_compatible(fdt, -1, "hypervisor,platform");
    if (root_node < 0) {
        safe_printf("not compatible with \"hypercorert\" found\n");
        return -1;
    }

    int node =
        fdt_node_offset_by_prop_value(fdt, root_node, "device_type", "memory", sizeof("memory"));
    if (node < 0) {
        safe_printf("no memory region found\n");
        panic("invalid device tree");
    }

    const char *name = fdt_get_name(fdt, node, NULL);
    if (fdt_get_reg_info(fdt, node, &mem_addr, &mem_size) < 0)
        safe_printf("memory fdt parse failed\n");
    cfg->memory.base = mem_addr;
    cfg->memory.size = mem_size;
    safe_printf("\"%s\" -> <%lx, 0x%lx>\n", name, mem_addr, mem_size);

    parse_host_gic_cfg(fdt, root_node, cfg);
    parse_host_uart_cfg(fdt, root_node, cfg);
    int guest_node = fdt_node_offset_by_compatible(fdt, root_node, "hypervisor,guest");
    parse_guest_cfg(fdt, guest_node, cfg);
    parse_timer_cfg(fdt, root_node, guest_node, cfg);
    validate_guest_dtb_config(cfg);

    safe_printf("hyper cfg: guest entry=%lx dtb=%lx vcpus=%d vgic=<%lx,%lx> timers=<%d,%d>\n",
                cfg->guest.entry,
                cfg->guest.dtb_addr,
                cfg->guest.vcpu_count,
                cfg->guest.vgic.gicd_base,
                cfg->guest.vgic.gicr_base,
                cfg->timer.hyp_timer_ppi,
                cfg->timer.guest_virt_timer_ppi);
    safe_printf("uart bridge: enabled=%d host=<%lx,%lx irq=%d> guest=<%lx,%lx irq=%d>\n",
                cfg->uart.enabled,
                cfg->uart.host_base,
                cfg->uart.host_size,
                cfg->uart.host_irq,
                cfg->uart.guest_base,
                cfg->uart.guest_size,
                cfg->uart.guest_irq);

#if 0
    iounmap_page(fdt);
#endif
    return 0;
}

int cpu_init(void) {
    /* do NOT use printf here */

    uint64_t id0 __attribute__((unused)) = mrs(ID_AA64MMFR0_EL1); /* refs: arm:D7-2336 */
    uint64_t id1 __attribute__((unused)) = mrs(ID_AA64MMFR1_EL1);

    // hyper_info("ID_AA64MMFR1_EL1: 0x%p, 0x%p", id0, id1);
    /*
    bits[3:0] Physical Address range supported. Defined values are::
        0000 32 bits, 4GB.
        0001 36 bits, 64GB.
        0010 40 bits, 1TB.
        0011 42 bits, 4TB.
        0100 44 bits, 16TB.
        0101 48 bits, 256TB.
        0110 52 bits, 4PB.

        All other values are reserved.
    */
    // id = id > 5 ? 5 : id;

    int phys_bits = get_phys_bits();
    // hyper_debug("support %d(%d) physical address",  get_phys_bits(), id2pa_range(id0 & 0xF),
    //         get_phys_id());
    uint64_t tcr_val = (TCR_RES1 | TCR_SH0_IS | TCR_ORGN0_WBWA |
                        TCR_IRGN0_WBWA | TCR_T0SZ(64 - phys_bits));
    tcr_val |= (2 << 16); // SL0 = 2 => lookup level is 0

    if (phys_bits < 40) {
        // hyper_err("phys bits:%d unsupported", phys_bits);
        return -1;
    }

    /* such as: 0x8084_3510 */
    msr(tcr_el2, tcr_val);

    /* Save for secondary CPU bring-up */
    g_mmu_boot.tcr_el2 = tcr_val;
    // hyper_info("TCR_EL2:%x", tcr_val);

    /* clear SCTLR.A */
    // msr_sync(SCTLR_EL2, SCTLR_EL2_SET);

    /*
     * Ensure that any exceptions encountered at EL2
     * are handled using the EL2 stack pointer, rather
     * than SP_EL0.
     */
    msr(spsel, 1);

    return 0;
}

void hyper_init_entry(void){


    safe_printf("init, current_el:%d\n", current_el());

    while (1) {
        wfi();
        // *(int*)0x10000000 = 'x';
        safe_printf("init wakeup at el%d\n", current_el());
        safe_printf("objdump:%llx\n", *((int *)hyper_config()->guest.entry));
    }
}

void hyper_guard(void) {
    safe_printf("guard\n");
    while (1) {
        wfi();
        safe_printf("guard wakeup\n");
    }
}
void hyper_idle(void) {
    safe_printf("idle\n");
    while (1) {
        wfi();
        safe_printf("idle wakeup at el:%d\n", current_el());
    }
}

void hyper_guest(void) {
    safe_printf("hyper guest, current_el:%d\n", current_el());
    void (*func_ptr)() = (void (*)())hyper_config()->guest.entry;

    func_ptr();
    while(1);
}

#define EL2_SET  (SCTLR_EL2_RES1 | SCTLR_EL2_EE_LE |\
			SCTLR_EL2_WXN_DIS | SCTLR_EL2_ICACHE_DIS |\
			SCTLR_EL2_SA_DIS | SCTLR_EL2_DCACHE_DIS |\
			SCTLR_EL2_ALIGN_DIS | SCTLR_EL2_MMU_DIS)

#if defined(CONFIG_BOARD_QEMU_VIRT)

#define GIC_GICD_BASE 0x08000000
#define GIC_GICC_BASE 0x08010000
#define GIC_GICR_BASE 0x080A0000

#elif defined(CONFIG_BOARD_FVP_AEMVA)

#define GIC_GICD_BASE 0x2f000000
#define GIC_GICC_BASE 0x2c000000
#define GIC_GICR_BASE 0x2f100000

#endif
/* GIC-600 specific register offsets */
#define GICR_PWRR			0x24U

/* GICR_PWRR fields */
#define PWRR_RDPD_SHIFT			0
#define PWRR_RDAG_SHIFT			1
#define PWRR_RDGPD_SHIFT		2
#define PWRR_RDGPO_SHIFT		3

#define PWRR_RDPD			(1U << PWRR_RDPD_SHIFT)
#define PWRR_RDAG			(1U << PWRR_RDAG_SHIFT)
#define PWRR_RDGPD			(1U << PWRR_RDGPD_SHIFT)
#define PWRR_RDGPO			(1U << PWRR_RDGPO_SHIFT)

/*
 * Values to write to GICR_PWRR register to power redistributor
 * for operating through the core (GICR_PWRR.RDAG = 0)
 */
#define PWRR_ON				(0U << PWRR_RDPD_SHIFT)
#define PWRR_OFF			(1U << PWRR_RDPD_SHIFT)



int init_el3() {

    uintptr_t gicr_base = GIC_GICR_BASE;


#ifdef CONFIG_BOARD_FVP_AEMVA
    /* enable cntcr */
    writel(0x2a430000 + 0, 1);
    /* config 100M base frq */
    msr(cntfrq_el0, 0x5f5e100);
#endif

    gicv3_init_el3(gicr_base);

    return 0;
}

void switch_to_el2(void *stack, void *entry, void *dtb_arg) {
    asm volatile("msr cptr_el3, xzr");

    msr(cptr_el2, CPTR_EL2_RES1);
    asm volatile("msr cntvoff_el2, xzr");

    init_el3();


    msr(sctlr_el2, EL2_SET);
    msr(hcr_el2, 0);
#if 1
    /* power on GICv3 for GIC600
    https://git.stikonas.eu/andrius/arm-trusted-firmware/commit/7a7fbb122ee3f66be81f34d58895939ef411e3f6
    */

    void *gicr_base = (void *)(uintptr_t)GIC_GICR_BASE;

    do {
        writel(gicr_base + GICR_PWRR, PWRR_ON);


    } while ((readl(gicr_base + GICR_PWRR) & PWRR_RDPD) != PWRR_ON);
#endif
    // hcr_val &= ~1;//disable vmmu;


    uint64_t tmp = 0;
    asm volatile("mov %0, sp\n\t"
                 " msr sp_el2, %0\n\t"
                 : "=r"(tmp)
                 : "r"(tmp)
                 : "memory");
    msr(spsr_el2, 0);

    uint64_t scr = mrs(scr_el3);
    tmp = (SCR_EL3_RW_AARCH64 | SCR_EL3_HCE_EN |\
			SCR_EL3_RES1 | SCR_EL3_NS_EN);
    scr |= 1;
    scr |= (1u << 10);
    scr &= ~(1ul << 3);

    msr(scr_el3, tmp);

    safe_printf("scr_el3:%x\n", tmp);


    tmp = (SPSR_EL_DEBUG_MASK | SPSR_EL_SERR_MASK |\
			SPSR_EL_IRQ_MASK | SPSR_EL_FIQ_MASK |\
			SPSR_EL_M_AARCH64 | SPSR_EL_M_EL2H);
    msr(spsr_el3, tmp);
    msr(elr_el3, entry);
    /* Move dtb_arg into x0 so __init_hyper_low_level receives it
     * as the first argument after ERET.  ERET does not modify GPRs. */
    asm volatile("mov x0, %0\n\t"
                 "eret\n"
                 :: "r"(dtb_arg) : "memory");
    while(1);
}

int el2_init(void) {
    safe_printf("current EL is't EL2\n");
    while (1)
        ;
}
void _reset(void);
int __init_hyper_low_level(void *args);
void __armv8_switch_to_el2(void *entry, uint64_t flags);
/*******************************************************************************
 * MPIDR macros
 ******************************************************************************/
#define MPIDR_MT_MASK		(ULL(1) << 24)
#define MPIDR_CPU_MASK		MPIDR_AFFLVL_MASK
#define MPIDR_CLUSTER_MASK	(MPIDR_AFFLVL_MASK << MPIDR_AFFINITY_BITS)
#define MPIDR_AFFINITY_BITS	U(8)
#define MPIDR_AFFLVL_MASK	ULL(0xff)
#ifndef MPIDR_AFF0_SHIFT
#define MPIDR_AFF0_SHIFT	U(0)
#endif
#define MPIDR_AFF1_SHIFT	U(8)
#define MPIDR_AFF2_SHIFT	U(16)
#define MPIDR_AFF3_SHIFT	U(32)
#define MPIDR_AFF_SHIFT(_n)	MPIDR_AFF##_n##_SHIFT
#define MPIDR_AFFINITY_MASK	ULL(0xff00ffffff)
#define MPIDR_AFFLVL_SHIFT	U(3)
#define MPIDR_AFFLVL0		ULL(0x0)
#define MPIDR_AFFLVL1		ULL(0x1)
#define MPIDR_AFFLVL2		ULL(0x2)
#define MPIDR_AFFLVL3		ULL(0x3)
#define MPIDR_AFFLVL(_n)	MPIDR_AFFLVL##_n
#define MPIDR_AFFLVL0_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF0_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL1_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF1_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL2_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF2_SHIFT) & MPIDR_AFFLVL_MASK)
#define MPIDR_AFFLVL3_VAL(mpidr) \
		(((mpidr) >> MPIDR_AFF3_SHIFT) & MPIDR_AFFLVL_MASK)

int init_hyper_low_level(void *args) {
    early_uart_init();

#if 0
    u64 core_id = smp_id();
    if (core_id != 0) {
        arch_spin_lock(&g_smp_lock);
        safe_printf("new core: \n");
        // safe_printf("\n");
        arch_spin_unlock(&g_smp_lock);
        while(1);

    }
    if (core_id == 0) {
        arch_spin_lock(&g_smp_lock);
        // safe_printf("core 0 up\n");
        // safe_printf("smp\n");
        arch_spin_unlock(&g_smp_lock);
        // while(1);
    }
#endif
    // while(1);
    safe_printf("current EL:%d\n", current_el());
    if (current_el() == 3) {
        /* swith to EL2 */
        // switch_to_el2(el2_init);
        isb();
        arch_mb();

        safe_printf("switch to EL2...\n");
        isb();
        arch_mb();
        switch_to_el2(NULL, (void *)__init_hyper_low_level, args);
        // __armv8_switch_to_el2(__init_hyper_low_level, 1);
        // asm volatile("msr ");
    }

    if (current_el() != 2) {

        safe_printf("current EL:%d is't EL2\n", current_el());
        return -1;
    }

    return __init_hyper_low_level(args);
}


static void init_platform(uintptr_t dtb_phys)
{
    safe_printf("init low level %x\n", readl((void *)0x80010000));
    load_dtb(dtb_phys, &g_hyper_config);
    write_sysreg(&__hyp_vectors, vbar_el2);
    safe_printf("z-bss done\n");
    cpu_init();
    safe_printf("cpu-init done\n");
}

static void init_memory(void)
{
    init_mm();
    safe_printf("mmu-init done\n");
    init_percpu_area();
    write_sysreg(&__hyp_vectors, vbar_el2);
    hyper_info("vttbr=0x%lx", mrs(VTTBR_EL2));
}

static void init_interrupt(void)
{
    void *gicd_base = (void *)ioremap_page(g_hyper_config.host_gic.gicd_base, MT_DEVICE_nGnRnE);
    void *gicc_base = (void *)ioremap_page(GIC_GICC_BASE, MT_NORMAL);
    void *gicr_base = (void *)ioremap(g_hyper_config.host_gic.gicr_base,
                                      g_hyper_config.host_gic.gicr_size, MT_DEVICE_nGnRnE);

    hyper_info("GICv - %x", readl(gicd_base + 4));
    init_gicv3(gicd_base, gicc_base, gicr_base);
    gic_vcpu_init_pcpu();
    ipi_pcpu_init();

    /* Save virtual GICR base for secondary CPU gicv3_pcpu_init() */
    g_hyper_config.host_gic.gicr_virt = (uintptr_t)gicr_base;

    iounmap_page((vaddr_t)gicd_base);
    iounmap_page((vaddr_t)gicc_base);
    /* Note: do NOT iounmap gicr_base — secondaries need the mapping */
}

static void init_secondary_cpus(uintptr_t dtb_phys)
{
    void *fdt_mapped = (void *)ioremap(dtb_phys, 0x10000, MT_NORMAL);
    smp_boot_secondaries(fdt_mapped);
    iounmap((vaddr_t)fdt_mapped, 0x10000);
}

static void init_guest(void)
{
    init_sched();
    init_emul_dev();
    timer_init();
    create_task("guest", (void *)g_hyper_config.guest.entry, 10);
    hyper_info("HyperCoreRT boot finished");
}

int __init_hyper_low_level(void *args)
{
    uintptr_t dtb_phys = (uintptr_t)args;

    init_platform(dtb_phys);
    init_memory();
    init_interrupt();
    init_secondary_cpus(dtb_phys);
    init_guest();

    while (1)
        ;
    return 0;
}

