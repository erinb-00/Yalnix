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
    TracePrintf(1, "tty_read: Attempting to read from Terminal %d, Buffer %p, Size %d\n", tty_id, buffer, size);
    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buffer == NULL || size <= 0) {
        TracePrintf(0, "tty_read: Invalid parameters for TTY %d\n", tty_id);
        return -1;
    }

    tty_struct_t *tty = &tty_struct[tty_id];
    pcb_t *pcb = GetCurrentProcess();
    if (tty->read_buffer_size == 0) {
        TracePrintf(0, "tty_read: No data to read from TTY %d\n", tty_id);
        return 0; // No data to read
    }
    int bytes_to_read = (size < tty->read_buffer_size) ? size : tty->read_buffer_size;
    char *temp_buffer = malloc(bytes_to_read);
    if (temp_buffer == NULL) {
        TracePrintf(0, "tty_read: Failed to allocate temporary buffer for TTY %d\n", tty_id);
        return -1; // Allocation failure
    }
    memcpy(temp_buffer, tty->read_buffer, bytes_to_read);
    pcb->read_buffer = temp_buffer;
    pcb->read_buffer_size = bytes_to_read;
    tty->read_buffer_size -= bytes_to_read;
    // Shift remaining data in the read buffer
    memmove(tty->read_buffer, tty->read_buffer + bytes_to_read, tty->read_buffer_size);    
    if (tty->read_buffer_size == 0) {
        free(tty->read_buffer);
        tty->read_buffer = NULL;
    }
    TracePrintf(1, "tty_read: Read %d bytes from TTY %d\n", bytes_to_read, tty_id);
    return bytes_to_read;
}

int tty_write(int tty_id, const void *buffer, int size) {
    TracePrintf(1, "tty_write: Attempting to write to Terminal %d, Buffer %p, Size %d\n", tty_id, buffer, size);
    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buffer == NULL || size <= 0) {
        TracePrintf(0, "tty_write: Invalid parameters for TTY %d\n", tty_id);
        return -1;
    }

    tty_struct_t *tty = &tty_struct[tty_id];
    pcb_t *pcb = GetCurrentProcess();

    pcb->tty_write_buffer = buffer;
    pcb->tty_write_buffer_size = size;
    
    if (tty->write_buffer_size + size > TERMINAL_MAX_LINE) {
        TracePrintf(0, "tty_write: Write buffer overflow for TTY %d\n", tty_id);
        return -1; // Buffer overflow
    }

    if (tty->write_buffer == NULL) {
        tty->write_buffer = malloc(TERMINAL_MAX_LINE);
        if (tty->write_buffer == NULL) {
            TracePrintf(0, "tty_write: Failed to allocate write buffer for TTY %d\n", tty_id);
            return -1; // Allocation failure
        }
        tty->write_buffer_size = 0;
    }

    memcpy(tty->write_buffer + tty->write_buffer_size, buffer, size);
    tty->write_buffer_size += size;

    // If no current writer, set this process as the current writer
    if (tty->current_writer == NULL) {
        tty->current_writer = pcb;
    }

    TracePrintf(1, "tty_write: Wrote %d bytes to TTY %d\n", size, tty_id);
    return size;
}