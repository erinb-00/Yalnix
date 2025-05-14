#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include "yalnix.h"    // for pte_t, UserContext, PAGESHIFT, KERNEL_STACK_BASE/LIMIT
#include "hardware.h"
#include "ykernel.h"

/* Number of pages in the kernel stack */
#define KSTACK_NPAGES \
    ((KERNEL_STACK_LIMIT >> PAGESHIFT) - (KERNEL_STACK_BASE >> PAGESHIFT))

typedef struct pcb {
    pte_t       *region1_pt;                /* Region 1 page table */
    int          pid;                       /* From helper_new_pid() */
    UserContext  uctxt;                     /* Saved user-mode context */
    unsigned int kstack_pfn[KSTACK_NPAGES]; /* PFNs for the kernel stack */
    struct pcb  *next;                      /* Ready/free list linkage */
    void* brk;
} PCB;

/* Allocate and initialize a new PCB with the given user page table */
PCB* CreatePCB(pte_t* user_page_table);

/* Set the PFN for the kernel stack at the given index */
//void set_kstack_pfn(PCB* pcb, int index, unsigned int pfn);

/* Initialize the user-mode context (PC & SP) */
//void set_userContext(PCB* pcb, UserContext* uctxt, void* pc, void* sp);

/* Retrieve the saved user context */
//UserContext get_userContext(PCB* pcb);

/* Get the user page table for this process */
//pte_t* get_userpt_process(PCB* pcb);

//void set_userContext_sp(PCB* pcb, void* sp);

#endif /* PROCESS_H */

