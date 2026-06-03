#pragma once
#include "riscv64_system.h"

#define STR(x) #x

#define csrr(csr)                                                                                  \
    ({                                                                                             \
        unsigned long __v;                                                                         \
        __asm__ __volatile__("csrr %0, " STR(csr) : "=r"(__v) : : "memory");                           \
        __v;                                                                                       \
    })

#define csrw(csr, val)                                                                             \
    ({                                                                                             \
        unsigned long __v = (unsigned long)(val);                                                  \
        __asm__ __volatile__("csrw " STR(csr) ", %0" : : "rK"(__v) : "memory");                        \
    })


#define csrs(csr, val)                                                                             \
    ({                                                                                             \
        unsigned long __v = (unsigned long)(val);                                                  \
        __asm__ __volatile__("csrs " STR(csr) ", %0" : : "rK"(__v) : "memory");                        \
    })

#define csrc(csr, val)                                                                             \
    ({                                                                                             \
        unsigned long __v = (unsigned long)(val);                                                  \
        __asm__ __volatile__("csrc " STR(csr) ", %0" : : "rK"(__v) : "memory");                        \
    })

static inline void __sfence_vma_all(void)
{
	 __asm__ __volatile("sfence.vma" : : : "memory");
}