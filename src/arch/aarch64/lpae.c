/*
 * Stage-1 (EL2) and stage-2 (guest) LPAE entry builders.
 *
 * The helpers in this file translate the project's coarse-grained
 * mem_type / mem_access pair into the architectural bit pattern of a
 * single 64-bit LPAE descriptor.  The entry layout itself lives in
 * mmu.h (lpae_t union); here we are purely concerned with picking
 * the right field values for the requested mapping flavour.
 */

#include "mm.h"
#include "page.h"
#include "lpae.h"


/* ----------------------------------------------------------------- *
 * Stage-1 (EL2) entries: hypervisor-side mappings of HyperCoreRT    *
 * code/data and the device aperture used by the EL2 driver bring-up *
 * code.                                                             *
 * ----------------------------------------------------------------- */

lpae_t make_stage1_entry(paddr_t pa, unsigned int attr_index)
{
    lpae_t e = (lpae_t){
        .pt = {
            .valid  = 1,        /* descriptor is present                  */
            .table  = 1,        /* table bit set for 4 KiB leaves         */
            .ai     = attr_index, /* index into MAIR_EL2                  */
            .ns     = 1,        /* hypervisor lives in non-secure world   */
            .up     = 1,        /* AP[1] = 1 (single-EL regime, RES1)     */
            .ro     = 0,        /* allow writes                           */
            .af     = 1,        /* skip access-flag tracking              */
            .ng     = 1,        /* not-global: simplifies TLB shootdown   */
            .contig = 0,
            .xn     = 0,        /* allow execute (.text needs it)         */
            .avail  = 0,
        },
    };

    /*
     * Pick shareability based on the cache attribute index.  Normal
     * Inner-Non-Cacheable Outer-Non-Cacheable is architecturally
     * Outer-Shareable (ARM ARM treats anything weaker than that as
     * UNPREDICTABLE), device memory we keep Outer-Shareable too so
     * the same fence sequence works for all MMIO drivers.  Everything
     * else (WT, WB, ...) is Inner-Shareable since the hypervisor
     * itself runs SMP-coherent.
     */
    switch (attr_index) {
    case MT_NORMAL_NC:
    case MT_DEVICE_nGnRnE:
    case MT_DEVICE_nGnRE:
        e.pt.sh = LPAE_SH_OUTER;
        break;
    default:
        e.pt.sh = LPAE_SH_INNER;
        break;
    }

    lpae_set_mfn(e, pa >> PAGE_SHIFT);
    return e;
}


/* ----------------------------------------------------------------- *
 * Stage-2 (guest) entries.                                          *
 *                                                                   *
 * A stage-2 descriptor takes its memory attribute from MemAttr[3:0] *
 * inside the descriptor (no MAIR), and its permissions from the     *
 * (S2AP, XN) bits — there is no separate "AP[1]" field as in        *
 * stage 1.  We pick MemAttr+SH based on mem_type, then narrow the   *
 * (read, write, xn, af) quadruple according to mem_access.          *
 * ----------------------------------------------------------------- */

static void stage2_apply_type(lpae_t *e, enum mem_type type)
{
    switch (type) {
    case MEM_DEVICE_NC:
        e->p2m.mattr = MATTR_MEM_NC;
        e->p2m.sh    = LPAE_SH_OUTER;
        break;
    case MEM_DEVICE:
        e->p2m.mattr = MATTR_DEV;
        e->p2m.sh    = LPAE_SH_OUTER;
        break;
    case MEM_NORMAL_RW:
    default:
        e->p2m.mattr = MATTR_MEM;
        e->p2m.sh    = LPAE_SH_INNER;
        break;
    }
}

static void stage2_apply_access(lpae_t *e, enum mem_access access)
{
    switch (access) {
    case MEM_ACCESS_RW:
        e->p2m.xn = 1;          /* data / MMIO — never execute            */
        break;
    case MEM_ACCESS_NONE:
        e->p2m.af = 0;          /* first touch faults out to EL2          */
        break;
    case MEM_ACCESS_RWX:
    default:
        /* permissive defaults set in the seed entry below */
        break;
    }
}

lpae_t make_stage2_entry(paddr_t pa, enum mem_type type, enum mem_access access)
{
    lpae_t e = (lpae_t){
        .p2m = {
            .valid = 1,
            .table = 1,
            .read  = 1,
            .write = 1,
            .af    = 1,
            .type  = (unsigned long)type,   /* shadow copy for inspection */
            .sbz1  = 0,
        },
    };

    stage2_apply_type(&e, type);
    stage2_apply_access(&e, access);

    lpae_set_mfn(e, pa >> PAGE_SHIFT);
    return e;
}

