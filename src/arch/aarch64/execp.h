#pragma once
#include "htypes.h"
union esr {
    uint32_t bits;
    struct {
        unsigned long iss:25;  /* Instruction Specific Syndrome */
        unsigned long len:1;   /* Instruction length */
        unsigned long ec:6;    /* Exception Class */
    };
    struct esr_iabt {
        unsigned long fsc:6;  /* Instruction fault status code */
        unsigned long res0:1;  /* RES0 */
        unsigned long s1ptw:1; /* Stage 2 fault during stage 1 translation */
        unsigned long res1:1;  /* RES0 */
        unsigned long eat:1;   /* External abort type */
        unsigned long fnv:1;   /* FAR not Valid */
        unsigned long res2:14;
        unsigned long len:1;   /* Instruction length */
        unsigned long ec:6;    /* Exception Class */
    } iabt; /* HSR_EC_INSTR_ABORT_* */

    struct esr_dabt {
        unsigned long fsc:6;  /* Data Fault Status Code */
        unsigned long write:1; /* Write / not Read */
        unsigned long s1ptw:1; /* Stage 2 fault during stage 1 translation */
        unsigned long cm:1; /* Cache Maintenance */
        unsigned long ea:1;   /* External Abort Type */
        unsigned long fnv:1;   /* FAR not Valid */

        unsigned long sbzp0:3;
        unsigned long ar:1;    /* Acquire Release */
        unsigned long sf:1;    /* Sixty Four bit register */

        unsigned long reg:5;   /* Register */
        unsigned long sign:1;  /* Sign extend */
        unsigned long size:2;  /* Access Size */
        unsigned long valid:1; /* Syndrome Valid */
        unsigned long len:1;   /* Instruction length */
        unsigned long ec:6;    /* Exception Class */
    } dabt; /* HSR_EC_DATA_ABORT_* */
};

uint64_t get_default_hcr_flags(void);
void panic(char* msg);
uint64_t get_default_hcr_flags(void);
void switch_to_el1(void);