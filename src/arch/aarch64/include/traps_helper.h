#pragma once
#include "processor.h"
#include "inline_asm.h"
#include "excep.h"

#ifdef __cplusplus
extern "C" {
#endif

void print_iss_detail(const union esr esr);

#ifdef __cplusplus
}
#endif