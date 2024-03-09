#pragma once
#include "gicv3.h"
/*******************************************************************************
 * GIC Distributor interface accessors
 ******************************************************************************/
/*
 * Wait for updates to:
 * GICD_CTLR[2:0] - the Group Enables
 * GICD_CTLR[7:4] - the ARE bits, E1NWF bit and DS bit
 * GICD_ICENABLER<n> - the clearing of enable state for SPIs
 */
static inline void gicd_wait_for_pending_write(uintptr_t gicd_base)
{
	while ((gicd_read_ctlr(gicd_base) & GICD_CTLR_RWP_BIT) != 0U) {
	}
}

static inline uint32_t gicd_read_pidr2(uintptr_t base)
{
	return mmio_read_32(base + GICD_PIDR2_GICV3);
}

static inline uint64_t gicd_read_irouter(uintptr_t base, unsigned int id)
{
	assert(id >= MIN_SPI_ID);
	return GICD_READ_64(IROUTE, base, id);
}

static inline void gicd_write_irouter(uintptr_t base,
				      unsigned int id,
				      uint64_t affinity)
{
	assert(id >= MIN_SPI_ID);
	GICD_WRITE_64(IROUTE, base, id, affinity);
}

static inline void gicd_clr_ctlr(uintptr_t base,
				 unsigned int bitmap,
				 unsigned int rwp)
{
	gicd_write_ctlr(base, gicd_read_ctlr(base) & ~bitmap);
	if (rwp != 0U) {
		gicd_wait_for_pending_write(base);
	}
}

static inline void gicd_set_ctlr(uintptr_t base,
				 unsigned int bitmap,
				 unsigned int rwp)
{
	gicd_write_ctlr(base, gicd_read_ctlr(base) | bitmap);
	if (rwp != 0U) {
		gicd_wait_for_pending_write(base);
	}
}

/*******************************************************************************
 * GIC Redistributor interface accessors
 ******************************************************************************/
static inline uint32_t gicr_read_ctlr(uintptr_t base)
{
	return mmio_read_32(base + GICR_CTLR);
}

static inline void gicr_write_ctlr(uintptr_t base, uint32_t val)
{
	mmio_write_32(base + GICR_CTLR, val);
}

static inline uint64_t gicr_read_typer(uintptr_t base)
{
	return mmio_read_64(base + GICR_TYPER);
}

static inline uint32_t gicr_read_waker(uintptr_t base)
{
	return mmio_read_32(base + GICR_WAKER);
}

static inline void gicr_write_waker(uintptr_t base, uint32_t val)
{
	mmio_write_32(base + GICR_WAKER, val);
}

/*
 * Wait for updates to:
 * GICR_ICENABLER0
 * GICR_CTLR.DPG1S
 * GICR_CTLR.DPG1NS
 * GICR_CTLR.DPG0
 * GICR_CTLR, which clears EnableLPIs from 1 to 0
 */
static inline void gicr_wait_for_pending_write(uintptr_t gicr_base)
{
	while ((gicr_read_ctlr(gicr_base) & GICR_CTLR_RWP_BIT) != 0U) {
	}
}

static inline void gicr_wait_for_upstream_pending_write(uintptr_t gicr_base)
{
	while ((gicr_read_ctlr(gicr_base) & GICR_CTLR_UWP_BIT) != 0U) {
	}
}

/* Private implementation of Distributor power control hooks */
void arm_gicv3_distif_pre_save(unsigned int rdist_proc_num);
void arm_gicv3_distif_post_restore(unsigned int rdist_proc_num);

/*******************************************************************************
 * GIC Redistributor functions for accessing entire registers.
 * Note: The raw register values correspond to multiple interrupt IDs and
 * the number of interrupt IDs involved depends on the register accessed.
 ******************************************************************************/

/*
 * Accessors to read/write GIC Redistributor ICENABLER0 register
 */
static inline unsigned int gicr_read_icenabler0(uintptr_t base)
{
	return mmio_read_32(base + GICR_ICENABLER0);
}

static inline void gicr_write_icenabler0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ICENABLER0, val);
}

/*
 * Accessors to read/write GIC Redistributor ICENABLER0 and ICENABLERE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_icenabler(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_ICENABLER + (reg_num << 2));
}

static inline void gicr_write_icenabler(uintptr_t base, unsigned int reg_num,
					unsigned int val)
{
	mmio_write_32(base + GICR_ICENABLER + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor ICFGR0, ICFGR1 registers
 */
static inline unsigned int gicr_read_icfgr0(uintptr_t base)
{
	return mmio_read_32(base + GICR_ICFGR0);
}

static inline unsigned int gicr_read_icfgr1(uintptr_t base)
{
	return mmio_read_32(base + GICR_ICFGR1);
}

static inline void gicr_write_icfgr0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ICFGR0, val);
}

static inline void gicr_write_icfgr1(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ICFGR1, val);
}

/*
 * Accessors to read/write GIC Redistributor ICFGR0, ICFGR1 and ICFGRE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_icfgr(uintptr_t base, unsigned int reg_num)
{
	return mmio_read_32(base + GICR_ICFGR + (reg_num << 2));
}

static inline void gicr_write_icfgr(uintptr_t base, unsigned int reg_num,
					unsigned int val)
{
	mmio_write_32(base + GICR_ICFGR + (reg_num << 2), val);
}

/*
 * Accessor to write GIC Redistributor ICPENDR0 register
 */
static inline void gicr_write_icpendr0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ICPENDR0, val);
}

/*
 * Accessor to write GIC Redistributor ICPENDR0 and ICPENDRE
 * register corresponding to its number
 */
static inline void gicr_write_icpendr(uintptr_t base, unsigned int reg_num,
					unsigned int val)
{
	mmio_write_32(base + GICR_ICPENDR + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor IGROUPR0 register
 */
static inline unsigned int gicr_read_igroupr0(uintptr_t base)
{
	return mmio_read_32(base + GICR_IGROUPR0);
}

static inline void gicr_write_igroupr0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_IGROUPR0, val);
}

/*
 * Accessors to read/write GIC Redistributor IGROUPR0 and IGROUPRE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_igroupr(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_IGROUPR + (reg_num << 2));
}

static inline void gicr_write_igroupr(uintptr_t base, unsigned int reg_num,
						unsigned int val)
{
	mmio_write_32(base + GICR_IGROUPR + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor IGRPMODR0 register
 */
static inline unsigned int gicr_read_igrpmodr0(uintptr_t base)
{
	return mmio_read_32(base + GICR_IGRPMODR0);
}

static inline void gicr_write_igrpmodr0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_IGRPMODR0, val);
}

/*
 * Accessors to read/write GIC Redistributor IGRPMODR0 and IGRPMODRE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_igrpmodr(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_IGRPMODR + (reg_num << 2));
}

static inline void gicr_write_igrpmodr(uintptr_t base, unsigned int reg_num,
				       unsigned int val)
{
	mmio_write_32(base + GICR_IGRPMODR + (reg_num << 2), val);
}

/*
 * Accessors to read/write the GIC Redistributor IPRIORITYR(E) register
 * corresponding to its number, 4 interrupts IDs at a time.
 */
static inline unsigned int gicr_ipriorityr_read(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_IPRIORITYR + (reg_num << 2));
}

static inline void gicr_ipriorityr_write(uintptr_t base, unsigned int reg_num,
						unsigned int val)
{
	mmio_write_32(base + GICR_IPRIORITYR + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor ISACTIVER0 register
 */
static inline unsigned int gicr_read_isactiver0(uintptr_t base)
{
	return mmio_read_32(base + GICR_ISACTIVER0);
}

static inline void gicr_write_isactiver0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ISACTIVER0, val);
}

/*
 * Accessors to read/write GIC Redistributor ISACTIVER0 and ISACTIVERE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_isactiver(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_ISACTIVER + (reg_num << 2));
}

static inline void gicr_write_isactiver(uintptr_t base, unsigned int reg_num,
					unsigned int val)
{
	mmio_write_32(base + GICR_ISACTIVER + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor ISENABLER0 register
 */
static inline unsigned int gicr_read_isenabler0(uintptr_t base)
{
	return mmio_read_32(base + GICR_ISENABLER0);
}

static inline void gicr_write_isenabler0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ISENABLER0, val);
}

/*
 * Accessors to read/write GIC Redistributor ISENABLER0 and ISENABLERE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_isenabler(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_ISENABLER + (reg_num << 2));
}

static inline void gicr_write_isenabler(uintptr_t base, unsigned int reg_num,
					unsigned int val)
{
	mmio_write_32(base + GICR_ISENABLER + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor ISPENDR0 register
 */
static inline unsigned int gicr_read_ispendr0(uintptr_t base)
{
	return mmio_read_32(base + GICR_ISPENDR0);
}

static inline void gicr_write_ispendr0(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_ISPENDR0, val);
}

/*
 * Accessors to read/write GIC Redistributor ISPENDR0 and ISPENDRE
 * register corresponding to its number
 */
static inline unsigned int gicr_read_ispendr(uintptr_t base,
						unsigned int reg_num)
{
	return mmio_read_32(base + GICR_ISPENDR + (reg_num << 2));
}

static inline void gicr_write_ispendr(uintptr_t base, unsigned int reg_num,
						unsigned int val)
{
	mmio_write_32(base + GICR_ISPENDR + (reg_num << 2), val);
}

/*
 * Accessors to read/write GIC Redistributor NSACR register
 */
static inline unsigned int gicr_read_nsacr(uintptr_t base)
{
	return mmio_read_32(base + GICR_NSACR);
}

static inline void gicr_write_nsacr(uintptr_t base, unsigned int val)
{
	mmio_write_32(base + GICR_NSACR, val);
}

/*
 * Accessors to read/write GIC Redistributor PROPBASER register
 */
static inline uint64_t gicr_read_propbaser(uintptr_t base)
{
	return mmio_read_64(base + GICR_PROPBASER);
}

static inline void gicr_write_propbaser(uintptr_t base, uint64_t val)
{
	mmio_write_64(base + GICR_PROPBASER, val);
}

/*
 * Accessors to read/write GIC Redistributor PENDBASER register
 */
static inline uint64_t gicr_read_pendbaser(uintptr_t base)
{
	return mmio_read_64(base + GICR_PENDBASER);
}

static inline void gicr_write_pendbaser(uintptr_t base, uint64_t val)
{
	mmio_write_64(base + GICR_PENDBASER, val);
}

/*******************************************************************************
 * GIC ITS functions to read and write entire ITS registers.
 ******************************************************************************/
static inline uint32_t gits_read_ctlr(uintptr_t base)
{
	return mmio_read_32(base + GITS_CTLR);
}

static inline void gits_write_ctlr(uintptr_t base, uint32_t val)
{
	mmio_write_32(base + GITS_CTLR, val);
}

static inline uint64_t gits_read_cbaser(uintptr_t base)
{
	return mmio_read_64(base + GITS_CBASER);
}

static inline void gits_write_cbaser(uintptr_t base, uint64_t val)
{
	mmio_write_64(base + GITS_CBASER, val);
}

static inline uint64_t gits_read_cwriter(uintptr_t base)
{
	return mmio_read_64(base + GITS_CWRITER);
}

static inline void gits_write_cwriter(uintptr_t base, uint64_t val)
{
	mmio_write_64(base + GITS_CWRITER, val);
}

static inline uint64_t gits_read_baser(uintptr_t base,
					unsigned int its_table_id)
{
	assert(its_table_id < 8U);
	return mmio_read_64(base + GITS_BASER + (8U * its_table_id));
}

static inline void gits_write_baser(uintptr_t base, unsigned int its_table_id,
					uint64_t val)
{
	assert(its_table_id < 8U);
	mmio_write_64(base + GITS_BASER + (8U * its_table_id), val);
}

/*
 * Wait for Quiescent bit when GIC ITS is disabled
 */
static inline void gits_wait_for_quiescent_bit(uintptr_t gits_base)
{
	assert((gits_read_ctlr(gits_base) & GITS_CTLR_ENABLED_BIT) == 0U);
	while ((gits_read_ctlr(gits_base) & GITS_CTLR_QUIESCENT_BIT) == 0U) {
	}
}
