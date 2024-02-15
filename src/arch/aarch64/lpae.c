
#include "lpae.h"


lpae_t make_p2m_table_entry(vaddr_t virt, int attr) {

    return mfn_to_p2m_entry(virt >> PAGE_SHIFT, attr, p2m_access_rwx);
}


lpae_t make_lpae_entry(mfn_t mfn, unsigned int attr)
{
    lpae_t e = (lpae_t) {
        .pt = {
            .valid = 1,           /* Mappings are present */
            .table = 1,           /* Set to 1 for links and 4k maps */
            .ai = attr,
            .ns = 1,              /* Hyp mode is in the non-secure world */
            .up = 1,              /* See below */
            .ro = 0,              /* Assume read-write */
            .af = 1,              /* No need for access tracking */
            .ng = 1,              /* Makes TLB flushes easier */
            .contig = 0,          /* Assume non-contiguous */
            .xn = 0,              /* No need to execute outside .text */
            .avail = 0,           /* Reference count for domheap mapping */
        }};
    /*
     * For EL2 stage-1 page table, up (aka AP[1]) is RES1 as the translation
     * regime applies to only one exception level (see D4.4.4 and G4.6.1
     * in ARM DDI 0487B.a). If this changes, remember to update the
     * hard-coded values in head.S too.
     */

    switch ( attr )
    {
    case MT_NORMAL_NC:
        /*
         * ARM ARM: Overlaying the shareability attribute (DDI
         * 0406C.b B3-1376 to 1377)
         *
         * A memory region with a resultant memory type attribute of Normal,
         * and a resultant cacheability attribute of Inner Non-cacheable,
         * Outer Non-cacheable, must have a resultant shareability attribute
         * of Outer Shareable, otherwise shareability is UNPREDICTABLE.
         *
         * On ARMv8 sharability is ignored and explicitly treated as Outer
         * Shareable for Normal Inner Non_cacheable, Outer Non-cacheable.
         */
        e.pt.sh = LPAE_SH_OUTER;
        break;
    case MT_DEVICE_nGnRnE:
    case MT_DEVICE_nGnRE:
        /*
         * Shareability is ignored for non-Normal memory, Outer is as
         * good as anything.
         *
         * On ARMv8 sharability is ignored and explicitly treated as Outer
         * Shareable for any device memory type.
         */
        e.pt.sh = LPAE_SH_OUTER;
        break;
    default:
        e.pt.sh = LPAE_SH_INNER;  /* Xen mappings are SMP coherent */
        break;
    }

    WARN_ON((mfn_to_maddr(mfn) & ~PADDR_MASK));

    lpae_set_mfn(e, mfn);

    return e;
}

/**********
 *
 * P2M
*/


static void p2m_set_permission(lpae_t *e, p2m_type_t t, p2m_access_t a)
{
    /* First apply type permissions */
    switch ( t )
    {
    case p2m_ram_rw:
        e->p2m.xn = 0;
        e->p2m.write = 1;
        break;

    case p2m_ram_ro:
        e->p2m.xn = 0;
        e->p2m.write = 0;
        break;

    case p2m_iommu_map_rw:
    case p2m_map_foreign_rw:
    case p2m_grant_map_rw:
    case p2m_mmio_direct_dev:
    case p2m_mmio_direct_nc:
    case p2m_mmio_direct_c:
        e->p2m.xn = 1;
        e->p2m.write = 1;
        break;

    case p2m_iommu_map_ro:
    case p2m_map_foreign_ro:
    case p2m_grant_map_ro:
    case p2m_invalid:
        e->p2m.xn = 1;
        e->p2m.write = 0;
        break;

    case p2m_max_real_type:
        // BUG();
        break;
    }

    /* Then restrict with access permissions */
    switch ( a )
    {
    case p2m_access_rwx:
        break;
    case p2m_access_wx:
        e->p2m.read = 0;
        break;
    case p2m_access_rw:
        e->p2m.xn = 1;
        break;
    case p2m_access_w:
        e->p2m.read = 0;
        e->p2m.xn = 1;
        break;
    case p2m_access_rx:
    case p2m_access_rx2rw:
        e->p2m.write = 0;
        break;
    case p2m_access_x:
        e->p2m.write = 0;
        e->p2m.read = 0;
        break;
    case p2m_access_r:
        e->p2m.write = 0;
        e->p2m.xn = 1;
        break;
    case p2m_access_n:
    case p2m_access_n2rwx:
        e->p2m.read = e->p2m.write = 0;
        e->p2m.xn = 1;
        break;
    }
}

lpae_t mfn_to_p2m_entry(mfn_t mfn, p2m_type_t t, p2m_access_t a)
{
    /*
     * sh, xn and write bit will be defined in the following switches
     * based on mattr and t.
     */

    lpae_t e = (lpae_t) {
        .p2m.af = 1,
        .p2m.read = 1,
        .p2m.table = 1,
        .p2m.valid = 1,
        .p2m.type = t,
        .p2m.sbz1 = 0,
    };


    // BUILD_BUG_ON(p2m_max_real_type > (1 << 4));

    switch ( t )
    {
    case p2m_mmio_direct_dev:
        e.p2m.mattr = MATTR_DEV;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    case p2m_mmio_direct_c:
        e.p2m.mattr = MATTR_MEM;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    /*
     * ARM ARM: Overlaying the shareability attribute (DDI
     * 0406C.b B3-1376 to 1377)
     *
     * A memory region with a resultant memory type attribute of Normal,
     * and a resultant cacheability attribute of Inner Non-cacheable,
     * Outer Non-cacheable, must have a resultant shareability attribute
     * of Outer Shareable, otherwise shareability is UNPREDICTABLE.
     *
     * On ARMv8 shareability is ignored and explicitly treated as Outer
     * Shareable for Normal Inner Non_cacheable, Outer Non-cacheable.
     * See the note for table D4-40, in page 1788 of the ARM DDI 0487A.j.
     */
    case p2m_mmio_direct_nc:
        e.p2m.mattr = MATTR_MEM_NC;
        e.p2m.sh = LPAE_SH_OUTER;
        break;

    default:
        e.p2m.mattr = MATTR_MEM;
        e.p2m.sh = LPAE_SH_INNER;
    }

    p2m_set_permission(&e, t, a);

    // ASSERT(!(mfn_to_maddr(mfn) & ~PADDR_MASK));

    lpae_set_mfn(e, mfn);

    return e;
}
