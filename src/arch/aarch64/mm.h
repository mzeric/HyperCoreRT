#pragma once

#define pfn_to_paddr(pfn) ((paddr_t)(pfn) << PAGE_SHIFT)
#define paddr_to_pfn(pa)  ((unsigned long)((pa) >> PAGE_SHIFT))
#define paddr_to_pdx(pa)  mfn_to_pdx(maddr_to_mfn(pa))
#define gfn_to_gaddr(gfn) pfn_to_paddr(gfn_x(gfn))
#define gaddr_to_gfn(ga)  _gfn(paddr_to_pfn(ga))
#define mfn_to_maddr(mfn) pfn_to_paddr(mfn_x(mfn))
#define maddr_to_mfn(ma)  _mfn(paddr_to_pfn(ma))
#define vmap_to_mfn(va)   maddr_to_mfn(virt_to_maddr((vaddr_t)va))
#define vmap_to_page(va)  mfn_to_page(vmap_to_mfn(va))