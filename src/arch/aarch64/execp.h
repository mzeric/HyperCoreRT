#pragma once
#include "htypes.h"
union esr {
    uint32_t bits;
    struct {
        unsigned long iss:25;  /* Instruction Specific Syndrome */
        unsigned long len:1;   /* Instruction length */
        unsigned long ec:6;    /* Exception Class */
    };
};