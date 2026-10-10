OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    /*
     * The kernel is loaded at physical address 1 MiB.
     * Paging is not being set up by this initial kernel,
     * so virtual and physical addresses must match.
     */
    . = 0x100000;

    .text :
    {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);

    .rodata :
    {
        *(.rodata .rodata.*)
    } :text

    . = ALIGN(0x1000);

    .data :
    {
        *(.data .data.*)
    } :data

    . = ALIGN(0x1000);

    .bss (NOLOAD) :
    {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
    }

    ASSERT(SIZEOF(.text) > 0, "Kernel has no executable code")
}
