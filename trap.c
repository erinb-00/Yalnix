/**
 * trap.c
 *
 * Implementation of interrupt, exception, and trap handler functions for Yalnix
 */

#include "trap.h"
#include "hardware.h"
#include "yalnix.h"
#include "kernel.h"
#include "process.h"
#include "syscalls.h"
#include "ykernel.h"      // for SYS_FORK, SYS_EXEC, … macros
#include <string.h>       // for memcpy
#include <stdbool.h>
#include "tty.h"
#include "ipc.h"
#include "sync_lock.h"
#include "sync_cvar.h"
#include <stdlib.h>
#include <yuser.h>

//======================================================================
// CP2: Trap handler function type
//======================================================================
TrapHandler interruptVector[TRAP_VECTOR_SIZE];

//======================================================================
// CP4: Delay helper function
//======================================================================
static void delay_helper(void* item, void *ctx, void* ctx2) {
    PCB* pcb = (PCB*)item;
    TracePrintf(0, "Delay helper called for PCB %d with delay %d\n", pcb->pid, pcb->num_delay);
    if (pcb->num_delay > 0) {
        pcb->num_delay--;
        // Move the PCB from blocked to ready
        if (pcb->num_delay == 0){
            pcb->state = PCB_READY; // Update state to ready
            queue_delete_node(blocked_processes, pcb);
            queue_add(ready_processes, pcb);
        }
    }
}

//======================================================================
// CP3: Trap handlers for clock switching
//====================================================================== 
void TrapClockHandler(UserContext *uctxt) {

    queue_iterate(blocked_processes, delay_helper, NULL, NULL);

    // 2) Decide which PCB to run next
    PCB *prev = currentPCB;

    if (prev != idlePCB && prev->state == PCB_READY) {
        queue_add(ready_processes, prev);
    } 

    PCB* next  = queue_get(ready_processes);
    if (next == NULL) {
        // If no ready processes, run the idle process
        next = idlePCB;
        TracePrintf(0, "TrapClockHandler: No ready processes, switching to idle process.\n");
    }

 
    // Save the user registers into the old PCB
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));

    // 3) Do the kernel-mode context switch: save prev’s KernelContext,
    //    remap stack pages & switch to next’s region1 PT, flush TLBs
    KernelContextSwitch(KCSwitch, prev, next);

    // 4) Update currentPCB (inside KCSwitch) and copy the new UserContext back
    memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));

}

//======================================================================
// CP3: Trap handler for kernel system calls
//======================================================================
void TrapKernelHandler(UserContext *uctxt) {
    // Save the user registers
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));

    int syscall = uctxt->code;
    int retval = ERROR;
    bool exit_flag = false;

    switch (syscall) {
        case YALNIX_FORK:
            TracePrintf(0, "\n=========\nYALNIX_FORK(1)\n=========\n");
            retval = user_Fork(uctxt);
            TracePrintf(0, "\n=========\nYALNIX_FORK(2)\n=========\n");
            TracePrintf(0, "YALNIX_FORK: retval = %d\n", retval);
            break;

        case YALNIX_EXEC: {
            TracePrintf(0, "\n=========\nYALNIX_EXEC(1)\n=========\n");
            // args in regs[0]=filename, regs[1]=argv
            char *filename = (char *)uctxt->regs[0];
            char **args    = (char **)uctxt->regs[1];
            retval = user_Exec(filename, args);
            TracePrintf(0, "\n=========\nYALNIX_EXEC(2)\n=========\n");
            break;
        }
      
        case YALNIX_WAIT: {
            TracePrintf(0, "\n=========\nYALNIX_WAIT(1)\n=========\n");
            int *status = (int *)uctxt->regs[0];
            retval = user_Wait(status);
            TracePrintf(0, "\n=========\nYALNIX_WAIT(2)\n=========\n");
            break;
        }

        case YALNIX_GETPID:
            TracePrintf(0, "\n=========\nYALNIX_GETPID(1)\n=========\n");
            retval = user_GetPid();
            TracePrintf(0, "\n=========\nYALNIX_GETPID(2)\n=========\n");
            break;

        case YALNIX_BRK: {
            TracePrintf(0, "\n=========\nYALNIX_BRK(1)\n=========\n");
            void *addr = (void *)uctxt->regs[0];
            retval = user_Brk(addr);
            TracePrintf(0, "\n=========\nYALNIX_BRK(2)\n=========\n");
            break;
        }

        case YALNIX_DELAY: {
            TracePrintf(0, "\n=========\nYALNIX_DELAY(1)\n=========\n");
            int ticks = uctxt->regs[0];
            retval = user_Delay(ticks);
            TracePrintf(0, "\n=========\nYALNIX_DELAY(2)\n=========\n");
            break;
        }

        case YALNIX_EXIT: {
            TracePrintf(0, "\n=========\nYALNIX_EXIT(1)\n=========\n");
            int status = uctxt->regs[0];
            user_Exit(status);
            exit_flag = true;
            TracePrintf(0, "\n=========\nYALNIX_EXIT(2)\n=========\n");
            break;
        }

        case YALNIX_TTY_READ: {
            TracePrintf(0, "\n=========\nYALNIX_TTY_READ(1)\n=========\n");
            int tty_id = uctxt->regs[0];
            void *buf = (void *)uctxt->regs[1];
            int size = uctxt->regs[2];
            retval = user_TtyRead(tty_id, buf, size);
            if (currentPCB->kernel_read_buffer != NULL && retval > 0){
                memcpy(buf, currentPCB->kernel_read_buffer, retval);
                free(currentPCB->kernel_read_buffer);
                currentPCB->kernel_read_buffer = NULL;
                currentPCB->kernel_read_buffer_size = 0;
            }
            TracePrintf(0, "\n=========\nYALNIX_TTY_READ(2)\n=========\n");
            break;
        }

        case YALNIX_TTY_WRITE: {
            TracePrintf(0, "\n=========\nYALNIX_TTY_WRITE(1)\n=========\n");
            int tty_id = uctxt->regs[0];
            void *buf = (void *)uctxt->regs[1];
            int size = uctxt->regs[2];
            retval = user_TtyWrite(tty_id, buf, size);
            TracePrintf(0, "\n=========\nYALNIX_TTY_WRITE(2)\n=========\n");
            break;
        }

        case YALNIX_PIPE_INIT: {
            TracePrintf(0, "\n=========\nYALNIX_PIPE_INIT(1)\n=========\n");
            int *pipe_idp = (int *)uctxt->regs[0];
            retval = PipeInit(pipe_idp);
            TracePrintf(0, "\n=========\nYALNIX_PIPE_INIT(2)\n=========\n");
            break;
        }

        case YALNIX_PIPE_READ: {
            TracePrintf(0, "\n=========\nYALNIX_PIPE_READ(1)\n=========\n");
            int pipe_id = uctxt->regs[0];
            void *buf = (void *)uctxt->regs[1];
            int len = uctxt->regs[2];
            retval = PipeRead(pipe_id, buf, len);
            TracePrintf(0, "\n=========\nYALNIX_PIPE_READ(2)\n=========\n");
            break;
        }
        
        case YALNIX_PIPE_WRITE: {
            TracePrintf(0, "\n=========\nYALNIX_PIPE_WRITE(1)\n=========\n");
            int pipe_id = uctxt->regs[0];
            void *buf = (void *)uctxt->regs[1];
            int len = uctxt->regs[2];
            retval = PipeWrite(pipe_id, buf, len);
            TracePrintf(0, "\n=========\nYALNIX_PIPE_WRITE(2)\n=========\n");
            break;
        }
        
        case YALNIX_LOCK_INIT: {
            TracePrintf(0, "\n=========\nYALNIX_LOCK_INIT(1)\n=========\n");
            int *lock_idp = (int *)uctxt->regs[0];
            retval = LockInit(lock_idp);
            TracePrintf(0, "\n=========\nYALNIX_LOCK_INIT(2)\n=========\n");
            break;
        }

        case YALNIX_LOCK_ACQUIRE: {
            TracePrintf(0, "\n=========\nYALNIX_LOCK_ACQUIRE(1)\n=========\n");
            int lock_id = uctxt->regs[0];
            retval = Acquire(lock_id);
            TracePrintf(0, "\n=========\nYALNIX_LOCK_ACQUIRE(2)\n=========\n");
            break;
        }

        case YALNIX_LOCK_RELEASE: {
            TracePrintf(0, "\n=========\nYALNIX_LOCK_RELEASE(1)\n=========\n");
            int lock_id = uctxt->regs[0];
            retval = Release(lock_id);
            TracePrintf(0, "\n=========\nYALNIX_LOCK_RELEASE(2)\n=========\n");
            break;
        }

        case YALNIX_CVAR_INIT: {
            TracePrintf(0, "\n=========\nYALNIX_CVAR_INIT(1)\n=========\n");
            int *cvar_idp = (int *)uctxt->regs[0];
            retval = CvarInit(cvar_idp);
            TracePrintf(0, "\n=========\nYALNIX_CVAR_INIT(2)\n=========\n");
            break;
        }
        
        case YALNIX_CVAR_SIGNAL: {
            TracePrintf(0, "\n=========\nYALNIX_CVAR_SIGNAL(1)\n=========\n");
            int cvar_id = uctxt->regs[0];
            retval = CvarSignal(cvar_id);
            TracePrintf(0, "\n=========\nYALNIX_CVAR_SIGNAL(2)\n=========\n");
            break;
        }

        case YALNIX_CVAR_BROADCAST: {
            TracePrintf(0, "\n=========\nYALNIX_CVAR_BROADCAST(1)\n=========\n");
            int cvar_id = uctxt->regs[0];
            retval = CvarBroadcast(cvar_id);
            TracePrintf(0, "\n=========\nYALNIX_CVAR_BROADCAST(2)\n=========\n");
            break;
        }

        case YALNIX_CVAR_WAIT: {
            TracePrintf(0, "\n=========\nYALNIX_CVAR_WAIT(1)\n=========\n");
            int cvar_id = uctxt->regs[0];
            int lock_id = uctxt->regs[1];
            retval = CvarWait(cvar_id, lock_id);
            TracePrintf(0, "\n=========\nYALNIX_CVAR_WAIT(2)\n=========\n");
            break;
        }

        case YALNIX_RECLAIM: {
            TracePrintf(0, "\n=========\nYALNIX_RECLAIM(1)\n=========\n");
            int pid = uctxt->regs[0];
            retval = Reclaim(pid);
            TracePrintf(0, "\n=========\nYALNIX_RECLAIM(2)\n=========\n");
            break;
        }

        default:
            TracePrintf(0, "TrapKernelHandler: unknown syscall 0x%x\n", syscall);
            Halt();
    }
    // Advance PC to avoid re‑issuing the syscall
    //currentPCB->uctxt.pc = (void *)((char *)currentPCB->uctxt.pc + 4);

    // Restore updated user registers back to the trap frame
    memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
    // Place return value in r0
    if (!exit_flag){
        uctxt->regs[0] = retval;
    }
}

//======================================================================
// CP4: Trap handler for memory access errors
//======================================================================
void TrapMemoryHandler(UserContext *uctxt) {

    if (uctxt->code == YALNIX_ACCERR){
        TracePrintf(0, "YALNIX_ACCERR: invalid permissions\n");
        user_Exit(ERROR);
    }

    if (uctxt->code == YALNIX_MAPERR){
        TracePrintf(0, "YALNIX_MAPPER: address not mapped\n");
    }
    TracePrintf(0, "TrapMemoryHandler: uctxt->addr: %p\n", uctxt->addr);
    // Save the user registers

    //Compute fault‐page, stack‐page, and heap‐page
    unsigned int fault = (unsigned int)uctxt->addr;
    unsigned int page = (fault - VMEM_1_BASE) >> PAGESHIFT;
    unsigned int spage = (((unsigned int)currentPCB->uctxt.sp - VMEM_1_BASE) >> PAGESHIFT);
    unsigned int heap_page = (((unsigned int)currentPCB->brk - VMEM_1_BASE) >> PAGESHIFT);


    // Check for implicit stack growth: in R1, below SP, above heap
    if (fault >= VMEM_1_BASE && page <= spage && (page >= heap_page)) {
        // Grow the stack one page at a time 
        for (int p = spage; p >= (int)page; p--) {
            int frame = get_free_frame();
            if (frame < 0){
                TracePrintf(0, "pid %d: out of memory\n", currentPCB->pid);
                Halt();
            }
            currentPCB->region1_pt[p].valid = 1;
            currentPCB->region1_pt[p].pfn   = frame;
            currentPCB->region1_pt[p].prot  = PROT_READ | PROT_WRITE;

            // Flush R1 TLB so new pages take effect
            WriteRegister(REG_TLB_FLUSH,  VMEM_1_BASE + (p << PAGESHIFT));
        }

        // Resume the faulting process 
        return;
    }

    // Illegal access → kill the process
    TracePrintf(0,"pid %d: illegal memory access at %p — aborting\n", currentPCB->pid, uctxt->addr);
    Halt();
}

//======================================================================
// CP5: Trap handler for TTY receive
//======================================================================
void TrapTtyReceiveHandler(UserContext *uctxt) {
    TracePrintf(1, "tty receive trap at addr %p\n", uctxt->addr);

    tty_t *tty = &tty_struct[uctxt->code];
    tty->read_buffer_size = tty->read_buffer_size + TtyReceive(uctxt->code, tty->read_buffer + tty->read_buffer_size, TERMINAL_MAX_LINE - tty->read_buffer_size);

    if (!queue_is_empty(tty->read_queue)){
        TracePrintf(1, "tty read queue was not empty\n");

        PCB *next = queue_get(tty->read_queue);
        if (next == NULL){
            TracePrintf(0, "tty read queue was empty\n");
            return;
        }
        int num_bytes = tty->read_buffer_size;
        if (next->read_buffer_size < tty->read_buffer_size){
            num_bytes = next->read_buffer_size;
        }

        char* read_buffer = malloc(num_bytes);
        if (read_buffer == NULL){
            num_bytes = 0;
        } else {
            memcpy(read_buffer, tty->read_buffer, num_bytes);
            next->kernel_read_buffer = read_buffer;
            next->kernel_read_buffer_size = num_bytes;
        }

        next->uctxt.regs[0] = num_bytes;

        if(num_bytes < tty->read_buffer_size){
            tty->read_buffer_size = tty->read_buffer_size - num_bytes;
            memmove(tty->read_buffer, tty->read_buffer + num_bytes, tty->read_buffer_size);
        } else {
            tty->read_buffer_size = 0;
        }

        TracePrintf(1, "next->state = PCB_READY\n");
        TracePrintf(1, "next->kernel_read_buffer = %s\n", next->kernel_read_buffer);
        TracePrintf(1, "next->kernel_read_buffer_size = %d\n", next->kernel_read_buffer_size);
        next->state = PCB_READY;
        queue_delete_node(blocked_processes, next);
        queue_add(ready_processes, next);
    }

    // FIXME: IS THERE ANYTHING MORE TO DO HERE?

}

//======================================================================
// CP5: Trap handler for TTY transmit
//======================================================================
void TrapTtyTransmitHandler(UserContext *uctxt) {
    TracePrintf(1, "tty transmit trap at addr %p\n", uctxt->addr);

    tty_t *tty = &tty_struct[uctxt->code];

    if (tty->write_buffer_position < tty->write_buffer_size){
        int num_bytes = tty->write_buffer_size - tty->write_buffer_position;
        if (num_bytes > TERMINAL_MAX_LINE){
            num_bytes = TERMINAL_MAX_LINE;
        }
        TtyTransmit(uctxt->code, tty->write_buffer + tty->write_buffer_position, num_bytes);
        tty->write_buffer_position = tty->write_buffer_position + num_bytes;
        return;
    }
    free(tty->write_buffer);
    tty->write_buffer = NULL;
    if (tty->current_writer != NULL){
        tty->current_writer->uctxt.regs[0] = tty->current_writer->write_buffer_size;
        tty->current_writer->state = PCB_READY;
        queue_delete_node(blocked_processes, tty->current_writer);
        queue_add(ready_processes, tty->current_writer);
        tty->current_writer = NULL;
    }

    tty->using = 0;
    if (!queue_is_empty(tty->write_queue)){
        tty->using = 1;
        PCB* next = queue_get(tty->write_queue);
        if (next == NULL){
            TracePrintf(0, "tty write queue was empty\n");
            return;
        }
        
        void* buf = (void*)uctxt->regs[1];
        tty->write_buffer = malloc(uctxt->regs[2]);

        if (tty->write_buffer == NULL) {
            TracePrintf(0, "start_tty_write: Failed to allocate write buffer for TTY %d\n", uctxt->code);
            next->uctxt.regs[0] = -1; // Indicate error in user context
            next->state = PCB_READY; // Set the process state to READY
            queue_add(ready_processes, next); // Requeue the process
            return; // Cannot write to TTY if write buffer allocation fails
        } else {

            memcpy(tty->write_buffer, buf, uctxt->regs[2]);
            tty->write_buffer_size = uctxt->regs[2];
            tty->write_buffer_position = 0; // Reset write position
            tty->current_writer = next; // Set the current writer to this process
            tty->write_buffer_position = uctxt->regs[2];

            if(uctxt->regs[2] > TERMINAL_MAX_LINE) {
                tty->write_buffer_position = TERMINAL_MAX_LINE;
            }

            TracePrintf(1, "start_tty_write: Process %d writing %d bytes to TTY %d\n", next->pid, tty->write_buffer_position, uctxt->code);

            // Set the user context to indicate the write operation
            TtyTransmit(uctxt->code, tty->write_buffer, tty->write_buffer_position);// Transmit the data to the terminal
        }
    }

}

//======================================================================
// CP6: Trap handler for disk errors
//======================================================================
void TrapDiskHandler(UserContext *uctxt) {
    TracePrintf(1, "YALNIX_DISK: %d THIS TRAP CAN BE IGNORED VIA DOCUMENTATION\n", uctxt->regs[0]); //FIXME: IS THIS WHERE I get my exit code from?
    user_Exit(uctxt->regs[0]);
}

//======================================================================
// CP6: Trap handler for math errors
//======================================================================
void TrapMathHandler(UserContext *uctxt) {
    TracePrintf(1, "YALNIX_MATH: %d\n", uctxt->regs[0]); 
    user_Exit(uctxt->regs[0]);
}

//======================================================================
// CP6: Trap handler for illegal instructions
//======================================================================
void TrapIllegalHandler(UserContext *uctxt) {
    TracePrintf(1, "YALNIX_ILLEGAL: %d\n", uctxt->regs[0]); 
    user_Exit(uctxt->regs[0]);
}

//======================================================================
// CP2: Initialize the trap handlers
//======================================================================
void TrapInit(void) {

    interruptVector[TRAP_CLOCK] = TrapClockHandler;
    interruptVector[TRAP_KERNEL] = TrapKernelHandler;
    interruptVector[TRAP_MEMORY] = TrapMemoryHandler;
    interruptVector[TRAP_ILLEGAL] = TrapIllegalHandler;
    interruptVector[TRAP_MATH] = TrapMathHandler;
    interruptVector[TRAP_DISK] = TrapDiskHandler;
    interruptVector[TRAP_TTY_RECEIVE] = TrapTtyReceiveHandler;
    interruptVector[TRAP_TTY_TRANSMIT] = TrapTtyTransmitHandler;

}
