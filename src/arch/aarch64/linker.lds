/*


*/

#include <autoconf.h>

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH("aarch64")
ENTRY(_start);

SECTIONS{

  . = CONFIG_ENTRY_ADDR;
  _hyper_start = .;
  .text :{
    _text_start_ = .;
    *(.text.vector)
    *(.text.*)
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

  .bss :  {
    . = ALIGN(4);
		PROVIDE(_bss_start = .);
    *(.bss)
    *(.bss.*)
    *(COMMON)
    . = ALIGN(4);

		PROVIDE(_bss_end = .);

  }

  /* reserse 1MB for printf/malloc */
  .dymem (NOLOAD): {
    . = ALIGN(8);
    _heap_low_ = .;
    . += 0x200000;
    _heap_top_ = .;
  }
.=ALIGN(0x1000);
	.hvc_stack :
	{
		PROVIDE(_hvc_stack_start = .);
		. = . + (CONFIG_INT_STACK_SIZE * CONFIG_SMP_CPU_NUM);
		. = ALIGN(8);
		PROVIDE(_hvc_stack_end = .);
	}

  _hyper_end = .;

  .guest_test_stack :
  {
    PROVIDE(_guest_stack = .);
    . += 0x10000;
    PROVIDE(_guest_stack_end = .);
  }
  /DISCARD/ :
  {
      *(.comment)
      *(.eh_frame)
      *(.note.GNU-stack)
  }

}
