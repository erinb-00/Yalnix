#include "tty.h"
#include "hardware.h"
#include "yalnix.h"
#include "ylib.h"
#include "process.h"
#include "kernel.h"


tty_struct_t tty_struct(NUM_TERMINALS);

void tty_init(void) {
    TracePrintf(1, "tty_init: Initializing TTY structures...\n");
    // Initialize each terminal's tty_struct
    for (int i = 0; i < NUM_TERMINALS; i++) {
        tty_struct[i].read_queue = pcb_queue_create();
        tty_struct[i].write_queue = pcb_queue_create();
        tty_struct[i].read_buffer = malloc(TERMINAL_MAX_LINE);
        if (tty_struct[i].read_buffer == NULL) {
            TracePrintf(0, "Failed to allocate read buffer for TTY %d\n", i);
            Halt();
        }
        tty_struct[i].read_buffer_size = 0;
        tty_struct[i].write_buffer = NULL;
        tty_struct[i].write_buffer_size = 0;
        tty_struct[i].current_writer = NULL;
        tty_struct[i].using = 0;
        TracePrintf(1, "tty_init: TTY %d initialized.\n", i);
    }
    TracePrintf(1, "tty_init: TTY structures initialized successfully.\n");
}

int tty_read(int tty_id, void *buffer, int size) {
    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buffer == NULL || size <= 0) {
        TracePrintf(0, "tty_read: Invalid parameters for TTY %d\n", tty_id);
        return -1;
    }

    tty_struct_t *tty = &tty_struct[tty_id];
    if (tty->read_buffer_size == 0) {
        TracePrintf(0, "tty_read: No data to read from TTY %d\n", tty_id);
        return 0; // No data to read
    }

    int bytes_to_read = (size < tty->read_buffer_size) ? size : tty->read_buffer_size;
    memcpy(buffer, tty->read_buffer, bytes_to_read);
    tty->read_buffer_size -= bytes_to_read;

    // Shift remaining data in the read buffer
    memmove(tty->read_buffer, tty->read_buffer + bytes_to_read, tty->read_buffer_size);

    TracePrintf(1, "tty_read: Read %d bytes from TTY %d\n", bytes_to_read, tty_id);
    return bytes_to_read;
}