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
#include "ykernel.h"

TrapHandler interruptVector[TRAP_VECTOR_SIZE];

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

void TrapClockHandler(UserContext *uctxt) {

    // 1) save out the user registers
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));

    // 2) decide who comes next (toggle each tick)
    PCB *prev = currentPCB;
    PCB *next = (prev == initPCB) ? idlePCB : initPCB;

    // 3) perform the context switch; KCSwitch will remap stacks
    KernelContextSwitch(KCSwitch, (void*)prev, (void*)next);

    // 4) update our pointer, restore user regs, and re‐install R1
    currentPCB = next;
    memcpy(uctxt, &next->uctxt, sizeof(UserContext));
    WriteRegister(REG_PTBR1, (unsigned int)next->region1_pt);
    WriteRegister(REG_PTLR1, MAX_PT_LEN);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

}

void TrapKernelHandler(UserContext *uctxt) {
  int syscall = uctxt->code;
    TracePrintf(1, "syscall code=0x%x\n", syscall);

    if (syscall == YALNIX_GETPID) {
      uctxt->regs[0] = getpid();
      return;  
    }

    if (syscall == YALNIX_BRK) {
      uctxt->regs[0] = brk((void*)uctxt->regs[0]);
      return;  
    }

    if (syscall == YALNIX_DELAY) {
      uctxt->regs[0] = Delay((int)uctxt->regs[0]);
      return;  
    }
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
    interruptVector[TRAP_MEMORY]  = TrapMemoryHandler;
}
