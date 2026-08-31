#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

void handle_keyboard_interrupt(unsigned char scancode);
int load_elf_program(const uint8_t *file_buf, uint32_t file_size, uint32_t *entry_out);
void enter_user_program(uint32_t user_entry);

/* Debug helper: auto-run the HELLO.ELF at boot for tracing mapping faults */
void auto_run_hello(void);
#endif