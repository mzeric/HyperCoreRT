ENTRY(_start);

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS{

  . = 0x90080000;
  .text :{
    _text_start_ = .;
    *(.text.vector)
    *(.text)
    _text_end_  = .;
    . = ALIGN(4);
  } :text
  . = ALIGN(0x1000);

  .rodata :
  {
    *(.rodata .rodata.*)
    . = ALIGN(8);
  } :text

  .data :  {
    *(.data)
    *(.data.*)
    *(.readmostly.data)
    *(*.data)
    . = ALIGN(8);
  } :data

  .bss (NOLOAD) :  {
    . = ALIGN(4);
		PROVIDE(_bss_start = .);
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
		PROVIDE(_bss_end = .);
  } :data

  /* reserse 1MB for printf/malloc */
  .dymem (NOLOAD): {
    . = ALIGN(8);
    _heap_low_ = .;
    . += 0x100000;
    _heap_top_ = .;
  } :data

  /* reverse 16KB stack */
  .stack (NOLOAD): {
    . = ALIGN(8);
    . += 0x4000 ;/* stack 16KB */
    PROVIDE(_stack_top_ = .);
  } :data
  .trap_stack (NOLOAD): {
    . = ALIGN(8);
    . += 0x4000 ;/* stack 16KB */
    PROVIDE(_trap_stack_top_ = .);
  } :data
  /* some compiler need this fix */
  .note.gnu.build-id : {
    *(.note.gnu.build-id)
  } :text
  /DISCARD/ :
  {
      *(.comment)
      *(.eh_frame)
      *(.note.GNU-stack)
  }

  PROVIDE(_hyper_end = .);

}
