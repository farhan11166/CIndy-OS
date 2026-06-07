ENTRY(start)

SECTIONS
{
    /* Load the kernel at 1 MiB – standard for protected-mode kernels */
    . = 1M;

    /* Multiboot header must come first */
    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    /* Executable code */
    .text ALIGN(4K) :
    {
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    /* Initialised read-write data */
    .data ALIGN(4K) :
    {
        *(.data)
    }

    /* Zero-initialised data (BSS) — IDT and other globals live here */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    /* Stack lives LAST so it can grow downward without hitting BSS data */
    .stack ALIGN(4K) :
    {
        *(.stack)
    }
}
