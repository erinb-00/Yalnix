#include "tty.h"
#include "hardware.h"
#include "yalnix.h"
#include "ylib.h"
#include "process.h"
#include "kernel.h"
#include "queue.h"


tty_t tty_struct[NUM_TERMINALS];

void TtyInit(void) {
    
    TracePrintf(1, "Tty_Init: Initializing TTY\n");

    for (int i = 0; i < NUM_TERMINALS; i++) {

        tty_struct[i].read_queue = queue_new();
        tty_struct[i].write_queue = queue_new();
        tty_struct[i].read_buffer = malloc(TERMINAL_MAX_LINE);

        if (tty_struct[i].read_buffer == NULL || tty_struct[i].read_queue == NULL || tty_struct[i].write_queue == NULL) {
            TracePrintf(0, "Tty_Init: Failed to allocate memory for TTY number:- %d\n", i);
            Halt();
        }

        tty_struct[i].read_buffer_size = 0;
        tty_struct[i].write_buffer_size = 0;
        tty_struct[i].using = 0;
        tty_struct[i].write_buffer = NULL;
        tty_struct[i].current_writer = NULL;
        TracePrintf(1, "tty_init: TTY %d initialized.\n", i);
    }
}

int user_TtyRead(int tty_id, void *buf, int len) {

    TracePrintf(1, "user_TtyRead: Attempting to read from Terminal %d, Buffer %p, Size %d\n", tty_id, buf, len);

    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0) {
        TracePrintf(0, "user_TtyRead: Invalid parameters for TTY %d\n", tty_id);
        return ERROR;
    }

    tty_t *tty = &tty_struct[tty_id];

    if (tty->read_buffer_size > 0){

        int num_bytes = tty->read_buffer_size;
        if (len < num_bytes) {
            num_bytes = len;
        }

        currentPCB->read_buffer = malloc(num_bytes);
        if (currentPCB->read_buffer == NULL) {
            TracePrintf(0, "user_TtyRead: Failed to allocate temporary buffer for TTY %d\n", tty_id);
            return ERROR; // Allocation failure
        }

        currentPCB->read_buffer_size = num_bytes;
        memcpy(currentPCB->read_buffer , tty->read_buffer, num_bytes);

        tty->read_buffer_size = tty->read_buffer_size - num_bytes;
        if (num_bytes < tty->read_buffer_size) {
            // If we read less than the available data, we need to keep the remaining data in the buffer
            TracePrintf(1, "user_TtyRead: Read %d bytes from TTY %d, remaining %d bytes in buffer\n", num_bytes, tty_id, tty->read_buffer_size);
            memmove(tty->read_buffer, tty->read_buffer + num_bytes, tty->read_buffer_size);
        } 

        TracePrintf(1, "tty_read: Read %d bytes from TTY %d\n", num_bytes, tty_id);
        return num_bytes;
    }

    // If no data is available, we need to wait for data to be written to the TTY
    TracePrintf(1, "tty_read: No data available in TTY %d, waiting for data...\n", tty_id);
    currentPCB->read_buffer = buf; // Set the read buffer in the PCB
    currentPCB->read_buffer_size = len; // Set the size of the read buffer in the PCB
    // Add the current process to the read queue
    queue_add(tty->read_queue, currentPCB); // Enqueue the process in the TTY's read queue
    // Block the process
    currentPCB->state = PCB_BLOCKED; // Set the process state to BLOCKED
    queue_add(blocked_processes, currentPCB); // Enqueue the process in the blocked processes queue
    TracePrintf(1, "tty_read: Process %d blocked waiting for data on TTY %d\n", currentPCB->pid, tty_id);

    // KC switch to the next process
    PCB *next_process;
    if (!queue_is_empty(ready_processes)) {
        next_process = queue_get(ready_processes);
    } else {
        next_process = idlePCB;
    }

    TracePrintf(1, "tty_write: Switching to next process %d\n", next_process->pid);
    KernelContextSwitch(KCSwitch, currentPCB, next_process); // Switch to the next process

    // Process will be resumed when data is available
    TracePrintf(1, "tty_read: Process %d resumed after data is available on TTY %d\n", currentPCB->pid, tty_id);
    //user_TtyRead(int tty_id, void *buf, int len)
    TracePrintf(1, "currentPCB->uctxt.regs[0] = %d\n", currentPCB->uctxt.regs[0]);
    return currentPCB->uctxt.regs[0]; // Return the number of bytes read

}

int user_TtyWrite(int tty_id, void *buf, int len) {

    TracePrintf(1, "user_TtyWrite: Attempting to write to Terminal %d, Buffer %p, Size %d\n", tty_id, buf, len);

    if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0) {
        TracePrintf(0, "user_TtyWrite: Invalid parameters for TTY %d\n", tty_id);
        return ERROR;
    }

    currentPCB->write_buffer = buf;
    currentPCB->write_buffer_size = len;

    tty_t *tty = &tty_struct[tty_id];

    if (!tty->using) {

        tty->using = 1;
        tty->write_buffer = malloc(len);

        if (tty->write_buffer == NULL) {
            TracePrintf(0, "start_tty_write: Failed to allocate write buffer for TTY %d\n", tty_id);
            currentPCB->uctxt.regs[0] = -1; // Indicate error in user context
            currentPCB->state = PCB_READY; // Set the process state to READY
            queue_add(ready_processes, currentPCB); // Requeue the process
            return ERROR; // Cannot write to TTY if write buffer allocation fails
        } else {

            memcpy(tty->write_buffer, buf, len);
            tty->write_buffer_size = len;
            tty->write_buffer_position = 0; // Reset write position
            tty->current_writer = currentPCB; // Set the current writer to this process
            tty->write_buffer_position = len;

            if(len > TERMINAL_MAX_LINE) {
                tty->write_buffer_position = TERMINAL_MAX_LINE;
            }

            TracePrintf(1, "start_tty_write: Process %d writing %d bytes to TTY %d\n", currentPCB->pid, tty->write_buffer_position, tty_id);

            // Set the user context to indicate the write operation
            TtyTransmit(tty_id, tty->write_buffer, tty->write_buffer_position);// Transmit the data to the terminal
        }

    } else {
        TracePrintf(1, "tty_write: TTY %d is already in use, adding process %d to write queue\n", tty_id, currentPCB->pid);
        queue_add(tty->write_queue, currentPCB);
    }

    currentPCB->state = PCB_BLOCKED;
    queue_add(blocked_processes, currentPCB);
    PCB *next_process;
    if (!queue_is_empty(ready_processes)) {
        next_process = queue_get(ready_processes);
    } else {
        next_process = idlePCB;
    }

    TracePrintf(1, "tty_write: Switching to next process %d\n", next_process->pid);
    KernelContextSwitch(KCSwitch, currentPCB, next_process); // Switch to the next process
    TracePrintf(1, "tty_write: Process %d resumed after write operation on TTY %d\n", currentPCB->pid, tty_id);

    return len;
}
