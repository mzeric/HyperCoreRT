#pragma once
#include "page.h"
#include "cpu_aarch64.h"
#include "vmmio.h"
#include "mm.h"
#include "cpu_inline_asm.h"

lpae_t mfn_to_p2m_entry(mfn_t mfn, p2m_type_t t, p2m_access_t a);

lpae_t make_lpae_entry(paddr_t phy_addr, unsigned int attr);
