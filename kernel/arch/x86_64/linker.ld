OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FLAGS(5);   /* RX */
    rodata  PT_LOAD FLAGS(4);   /* R  */
    data    PT_LOAD FLAGS(6);   /* RW */
}

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(0x1000) {
        *(.text*)
    } :text

    .rodata : ALIGN(0x1000) {
        *(.rodata*)
    } :rodata

    .limine_requests_start : ALIGN(0x1000) {
        KEEP(*(.limine_requests_start))
    } :data

    .limine_requests : {
        KEEP(*(.limine_requests))
    } :data

    .limine_requests_end : {
        KEEP(*(.limine_requests_end))
    } :data

    .data : ALIGN(0x1000) {
        *(.data*)
    } :data

    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
    } :data
}