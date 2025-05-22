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

TrapHandler interruptVector[TRAP_VECTOR_SIZE];


void TrapClockHandler(UserContext *uctxt) {
   /* 
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
    WriteRegister(REG_PTBR1, (unsigned int)next->region1_pt);
    WriteRegister(REG_PTLR1, MAX_PT_LEN);
    currentPCB = next;

    */
}

void TrapKernelHandler(UserContext *uctxt) {
    int syscall = uctxt->code;
    TracePrintf(1, "syscall code=0x%x\n", syscall);
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
