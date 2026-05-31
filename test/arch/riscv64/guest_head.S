.section .text.vector
.global _start

_start:
    la sp, _stack_top_
    j _reset
