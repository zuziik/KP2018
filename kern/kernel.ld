OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS {
	. = KERNEL_LMA;
    entry = KERNEL_VMA + .;

	.boot ALIGN(4K) : {
		*/kern/boot.o (.text)
		*/kern/boot.o (.data)
		*/kern/boot.o (.bss)
        */kern/boot_ap.o (.text)
        */kern/boot_ap.o (.data)
        */kern/boot_ap.o (.bss)
		boot_end = .;
	}

    . += KERNEL_VMA;

	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA) {
		*(.text)
	}

    etext = .;

	.rodata ALIGN (4K) : AT(ADDR(.rodata) - KERNEL_VMA) {
		*(.rodata)
	}

    .data ALIGN (4K) : AT(ADDR(.data) - KERNEL_VMA) {
		*(.data)
	}

    edata = .;

	.bss ALIGN (4K) : AT(ADDR(.bss) - KERNEL_VMA) {
		*(.bss)
		*(COMMON)
	}

    end = .;
}

