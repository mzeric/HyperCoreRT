OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH("aarch64")
ENTRY(_start);

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 128M
}

SECTIONS{

  . = ORIGIN(RAM);
  . = ALIGN(0x1000);
  .text :{
    _text_start_ = .;
    *(.text.vector)
    *(.text)
    _text_end_  = .;
    . = ALIGN(4);
  }
  . = ALIGN(0x1000);

  .rodata :
  {
    *(.rodata .rodata.*)
    . = ALIGN(8);
  }

  .data :  {
    *(.data)
    *(.data.*)
    *(.readmostly.data)
    *(*.data)
    . = ALIGN(8);
  }

  .bss (NOLOAD) :  {
    . = ALIGN(4);
    _szero = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ezero = .;
  }

  /* reserse 1MB for printf/malloc */
  .dymem (NOLOAD): {
    . = ALIGN(8);
    _heap_low_ = .;
    . += 0x100000;
    _heap_top_ = .;
  }

  /* reverse 16KB stack */
  .stack (NOLOAD): {
    . = ALIGN(8);
    . += 0x4000 ;/* stack 16KB */
    _stack_top_ = .;
  }

  /* some compiler need this fix */
  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  }
  /DISCARD/ :
  {
      *(.comment)
      *(.eh_frame)
      *(.note.GNU-stack)
  }

}
