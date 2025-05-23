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

TrapHandler interruptVector[TRAP_VECTOR_SIZE];


void TrapClockHandler(UserContext *uctxt) {
 
    // Save the user registers into the old PCB
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));
    
    // 2) Decide which PCB to run next
    PCB *prev = currentPCB;
    PCB *next = (prev == idlePCB ? initPCB : idlePCB);

    // 3) Do the kernel-mode context switch: save prev’s KernelContext,
    //    remap stack pages & switch to next’s region1 PT, flush TLBs
    KernelContextSwitch(KCSwitch,
                       prev,
                       next);

    // 4) Update currentPCB (inside KCSwitch) and copy the new UserContext back
    memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));

}

void TrapKernelHandler(UserContext *uctxt) {
    // Save the user registers
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));

    int syscall = uctxt->code;
    int retval = ERROR;

    switch (syscall) {
        case YALNIX_FORK:
            // Fork fills in uctxt for parent and child
            retval = Fork(uctxt);
            break;

        case YALNIX_EXEC: {
            // args in regs[0]=filename, regs[1]=argv
            char *filename = (char *)currentPCB->uctxt.regs[0];
            char **args    = (char **)currentPCB->uctxt.regs[1];
            retval = Exec(filename, args);
            if (retval != ERROR) {
                // On success, new program context is in currentPCB->uctxt
                memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
                return;
            }
            break;
        }
      
        case YALNIX_WAIT: {
            int *status = (int *)currentPCB->uctxt.regs[0];
            break;
        }

        case YALNIX_GETPID:
            retval = GetPid();
            break;

        case YALNIX_BRK: {
            void *addr = (void *)currentPCB->uctxt.regs[0];
            retval = Brk(addr);
            break;
        }

        case YALNIX_DELAY: {
            int ticks = currentPCB->uctxt.regs[0];
            retval = Delay(ticks);
            break;
        }

        default:
            TracePrintf(0, "TrapKernelHandler: unknown syscall 0x%x\n", syscall);
    }

    // Place return value in r0
    currentPCB->uctxt.regs[0] = retval;
    // Advance PC to avoid re‑issuing the syscall
    currentPCB->uctxt.pc = (void *)((char *)currentPCB->uctxt.pc + 4);

    // Restore updated user registers back to the trap frame
    memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
}

void TrapDefaultHandler(UserContext *uctxt) {
    TracePrintf(1, "unhandled trap vector %d code %d addr %p\n",
                uctxt->vector, uctxt->code, uctxt->addr);
    Halt();
}

void TrapInit(void) {
    
    for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
        interruptVector[i] = TrapDefaultHandler;
    }

    
    interruptVector[TRAP_CLOCK] = TrapClockHandler;
    interruptVector[TRAP_KERNEL] = TrapKernelHandler;
}
