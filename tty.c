#include "tty.h"
#include "hardware.h"
#include "yalnix.h"
#include "ylib.h"
#include "process.h"
#include "kernel.h"


tty_struct_t tty_struct(NUM_TERMINALS);

void TtyInit(void) {
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

int SysTtyRead(int tty_id, void *buffer, int size) {
    TracePrintf(1, "tty_read: Attempting to read from Terminal %d, Buffer %p, Size %d\n", tty_id, buffer, size);
    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buffer == NULL || size <= 0) {
        TracePrintf(0, "tty_read: Invalid parameters for TTY %d\n", tty_id);
        return -1;
    }

    tty_struct_t *tty = &tty_struct[tty_id];
    pcb_t *pcb = GetCurrentProcess();
    // Copy immediately
    if (tty->read_buffer_size > 0){
        int bytes_to_read = (size < tty->read_buffer_size) ? size : tty->read_buffer_size;
        TracePrintf(1, "tty_read: Reading %d bytes from TTY %d\n", bytes_to_read, tty_id);

        // Copy data from the read buffer to the provided buffer
        char *temp_buffer = malloc(bytes_to_read);
        if (temp_buffer == NULL) {
            TracePrintf(0, "tty_read: Failed to allocate temporary buffer for TTY %d\n", tty_id);
            return -1; // Allocation failure
        }
        memcpy(temp_buffer, tty->read_buffer, bytes_to_read);
        pcb->read_buffer = temp_buffer;
        pcb->read_buffer_size = bytes_to_read;

        if (bytes_to_read < tty->read_buffer_size) {
            // If we read less than the available data, we need to keep the remaining data in the buffer
            TracePrintf(1, "tty_read: Read %d bytes from TTY %d, remaining %d bytes in buffer\n", bytes_to_read, tty_id, tty->read_buffer_size - bytes_to_read);
            tty->read_buffer_size -= bytes_to_read;

            memmove(tty->read_buffer, tty->read_buffer + bytes_to_read, tty->read_buffer_size);
        } 
        tty->read_buffer_size -= bytes_to_read; // Update the read buffer size
        TracePrintf(1, "tty_read: Read %d bytes from TTY %d\n", bytes_to_read, tty_id);
        return bytes_to_read;
    }
    // If no data is available, we need to wait for data to be written to the TTY
    TracePrintf(1, "tty_read: No data available in TTY %d, waiting for data...\n", tty_id);
    pcb->tty_read_buffer = buffer; // Set the read buffer in the PCB
    pcb->tty_read_buffer_size = size; // Set the size of the read buffer in the PCB
    // Add the current process to the read queue
    pcb_enqueue(tty->read_queue, pcb); // Enqueue the process in the TTY's read queue
    // Block the process
    pcb->state = PCB_BLOCKED; // Set the process state to BLOCKED
    pcb_enqueue(blocked_processes, pcb); // Enqueue the process in the blocked processes queue
    TracePrintf(1, "tty_read: Process %d blocked waiting for data on TTY %d\n", pcb->pid, tty_id);
    // KC switch to the next process
    pcb_t *next_process = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
    TracePrintf(1, "tty_read: Switching to next process %d\n", next_process->pid);
    KernelContextSwitch(KCSwitch, pcb, next);
    // Process will be resumed when data is available
    TracePrintf(1, "tty_read: Process %d resumed after data is available on TTY %d\n", pcb->pid, tty_id);
    return pcb->user_context.regs[0]; // Return the number of bytes read
    // If there is data available, we can read it immediately

}
void start_tty_write(int terminal, pcb_t *writer, void *buffer, int size) {
    TracePrintf(1, "start_tty_write: Starting write to Terminal %d by process %d\n", terminal, writer->pid);
    if (terminal < 0 || terminal >= NUM_TERMINALS || writer == NULL || buffer == NULL || size <= 0) {
        TracePrintf(0, "start_tty_write: Invalid parameters for TTY %d\n", terminal);
        return;
    }
    tty_struct_t *tty = &tty_struct[terminal];
    tty->write_buffer = malloc(size);
    if (tty->write_buffer == NULL) {
        TracePrintf(0, "start_tty_write: Failed to allocate write buffer for TTY %d\n", terminal);
        writer->user_context.regs[0] = -1; // Indicate error in user context
        writer->state = PCB_READY; // Set the process state to READY
        pcb_enqueue(ready_processes, writer); // Requeue the process
        return; // Cannot write to TTY if write buffer allocation fails
    }
    if (tty->using && tty->current_writer != writer) {
        TracePrintf(0, "start_tty_write: TTY %d is already in use by another process %d\n", terminal, tty->current_writer->pid);
        return; // TTY is already in use by another process
    }
    memcpy(tty->write_buffer, buffer, size);
    tty->write_buffer_size = size;
    tty->write_buffer_position = 0; // Reset write position
    tty->current_writer = writer; // Set the current writer to this process

    int write_pos = (len > TERMINAL_MAX_LINE) ? TERMINAL_MAX_LINE : len;
    TracePrintf(1, "start_tty_write: Process %d writing %d bytes to TTY %d\n", writer->pid, write_pos, terminal);
    // Set the user context to indicate the write operation
    TtyTransmit(terminal, tty->write_buffer, write_pos); // Transmit the data to the terminal
    tty->write_buffer_position = write_pos;
}
int SysTtyWrite(int tty_id, const void *buffer, int size) {
    TracePrintf(1, "tty_write: Attempting to write to Terminal %d, Buffer %p, Size %d\n", tty_id, buffer, size);

    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buffer == NULL || size <= 0) {
        TracePrintf(0, "tty_write: Invalid parameters for TTY %d\n", tty_id);
        return -1;
    }

    tty_struct_t *tty = &tty_struct[tty_id];
    pcb_t *pcb = GetCurrentProcess();

    pcb->tty_write_buffer = buffer;
    pcb->tty_write_buffer_size = size;

    if (!tty->using) {
        tty->using = 1; // Mark the TTY as being used
        TracePrintf(1, "tty_write: TTY %d is now in use by process %d\n", tty_id, pcb->pid);
        start_tty_write(tty_id, pcb, (void *)buffer, size);
    } else {
        TracePrintf(1, "tty_write: TTY %d is already in use, adding process %d to write queue\n", tty_id, pcb->pid);
        pcb_enqueue(tty->write_queue, pcb); // Add the process to the write queue
    }
    pcb->state = PCB_BLOCKED; // Set the process state to BLOCKED
    pcb_enqueue(blocked_processes, pcb); // Enqueue the process in the blocked processes queue

    pcb_t *next_process = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
    TracePrintf(1, "tty_write: Switching to next process %d\n", next_process->pid);
    KernelContextSwitch(KCSwitch, pcb, next_process); // Switch to the next process
    TracePrintf(1, "tty_write: Process %d resumed after write operation on TTY %d\n", pcb->pid, tty_id);
    return size; // Return the number of bytes written
}
