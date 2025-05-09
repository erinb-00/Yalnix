/**
 * trap.c
 *
 * Implementation of interrupt, exception, and trap handler functions for Yalnix
 */

 #include "trap.h"
 #include "hardware.h"
 #include "yalnix.h"

TrapHandler interruptVector[TRAP_VECTOR_SIZE];

void TrapClockHandler(UserContext *uctxt) {
    TracePrintf(1, "clock interrupt\n");
   
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
