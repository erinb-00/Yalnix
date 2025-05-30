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

//======================================================================
// CP2: Trap handler function type
//======================================================================
TrapHandler interruptVector[TRAP_VECTOR_SIZE];

static void delay_helper(PCB *pcb, void *ctx) {
    TracePrintf(0, "Delay helper called for PCB %d with delay %d\n", pcb->pid, pcb->num_delay);
    if (pcb->num_delay > 0) {
        pcb->num_delay--;
    }
    if (pcb->num_delay == 0) {
        // Move the PCB from blocked to ready
        queue_delete_node(blocked_processes, pcb);
        queue_add(ready_processes, pcb);
        pcb->state = PCB_READY; // Update state to ready
    }
}

//======================================================================
// CP3: Trap handlers for clock switching
//====================================================================== 
void TrapClockHandler(UserContext *uctxt) {

    queue_iterate(blocked_processes, delay_helper, NULL);

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
            char *filename = (char *)currentPCB->uctxt.regs[0];
            char **args    = (char **)currentPCB->uctxt.regs[1];
            retval = user_Exec(filename, args);
            if (retval != ERROR) {
                // On success, new program context is in currentPCB->uctxt
                memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
            }
            TracePrintf(0, "\n=========\nYALNIX_EXEC(2)\n=========\n");
            break;
        }
      
        case YALNIX_WAIT: {
            TracePrintf(0, "\n=========\nYALNIX_WAIT(1)\n=========\n");
            int *status = (int *)currentPCB->uctxt.regs[0];
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
            void *addr = (void *)currentPCB->uctxt.regs[0];
            retval = user_Brk(addr);
            TracePrintf(0, "\n=========\nYALNIX_BRK(2)\n=========\n");
            break;
        }

        case YALNIX_DELAY: {
            TracePrintf(0, "\n=========\nYALNIX_DELAY(1)\n=========\n");
            int ticks = currentPCB->uctxt.regs[0];
            retval = user_Delay(ticks);
            TracePrintf(0, "\n=========\nYALNIX_DELAY(2)\n=========\n");
            break;
        }

        case YALNIX_EXIT: {
            TracePrintf(0, "\n=========\nYALNIX_EXIT(1)\n=========\n");
            int status = currentPCB->uctxt.regs[0];
            user_Exit(status);
            exit_flag = true;
            TracePrintf(0, "\n=========\nYALNIX_EXIT(2)\n=========\n");
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
// CP2: Default trap handler for unhandled traps
//======================================================================
void TrapDefaultHandler(UserContext *uctxt) {
    TracePrintf(1, "In TrapDeaulf Handler: unhandled trap vector %d code %d addr %p\n",
                uctxt->vector, uctxt->code, uctxt->addr);
    Halt();
}

void TrapMemoryHandler(UserContext *uctxt) {
    TracePrintf(1, "memory trap at addr %p\n", uctxt->addr);

    /* 1) Save incoming user registers */
    currentPCB->uctxt = *uctxt;

    /* 2) Compute fault‐page, stack‐page, and heap‐page */
    unsigned int fault     = (unsigned int)uctxt->addr;
    unsigned int page      = (fault - VMEM_1_BASE) >> PAGESHIFT;
    unsigned int spage     = (((unsigned int)currentPCB->uctxt.sp
                               - VMEM_1_BASE) >> PAGESHIFT);
    unsigned int heap_page = (((unsigned int)currentPCB->brk
                               - VMEM_1_BASE) >> PAGESHIFT);

    /* 3) Check for implicit stack growth:
     *    in R1, below SP, above heap
     */
    if (fault >= VMEM_1_BASE &&
        page < spage &&
        page >= heap_page) 
    {
        /* Grow the stack one page at a time */
        for (int p = spage - 1; p >= (int)page; p--) {
            int frame = get_free_frame();
            if (frame < 0) goto abort;    // out of memory
            currentPCB->region1_pt[p].valid = 1;
            currentPCB->region1_pt[p].pfn   = frame;
            currentPCB->region1_pt[p].prot  = PROT_READ | PROT_WRITE;
        }
        /* Flush R1 TLB so new pages take effect */
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

        /* 4) Resume the faulting process */
        *uctxt = currentPCB->uctxt;
        return;
    }

abort:
    /* 5) Illegal access → kill the process */
    TracePrintf(0,
        "pid %d: illegal memory access at %p — aborting\n",
        currentPCB->pid, uctxt->addr);
    exit(1);
}

//======================================================================
// CP2: Initialize the trap handlers
//======================================================================
void TrapInit(void) {
    
    for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
        interruptVector[i] = TrapDefaultHandler;
    }

    
    interruptVector[TRAP_CLOCK] = TrapClockHandler;
    interruptVector[TRAP_KERNEL] = TrapKernelHandler;
    interruptVector[TRAP_MEMORY] = TrapMemoryHandler;
}
