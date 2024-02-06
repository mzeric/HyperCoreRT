.global _start
.global _reset

.section .text.vector

_start:

    /* init stack */
    lui sp, %hi(_stack_top_)
    addi sp, sp, %lo(_stack_top_)

    la sp, _stack_top_ # FIXME

    j _reset

    /* should never be here */
    j .
