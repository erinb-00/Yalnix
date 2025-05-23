#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include "yalnix.h"    // for pte_t, UserContext, PAGESHIFT, KERNEL_STACK_BASE/LIMIT
#include "hardware.h"
#include "ykernel.h"
#include "queue.h"      // for queue_t to track child PCBs

/* Number of pages in the kernel stack */
#define KSTACK_NPAGES \
    ((KERNEL_STACK_LIMIT >> PAGESHIFT) - (KERNEL_STACK_BASE >> PAGESHIFT))

typedef struct pcb {
    pte_t       *region1_pt;                /* Region 1 page table */
    int          pid;                       /* From helper_new_pid() */
    UserContext  uctxt;                     /* Saved user-mode context */
    unsigned int kstack_pfn[KSTACK_NPAGES]; /* PFNs for the kernel stack */
    struct pcb  *next;                      /* Ready/free list linkage */
    void        *brk;                       /* Current break (heap) */
    KernelContext kctxt;                    /* Saved kernel-mode context */
    int         exit_status;                /* Exit status for the process */

    /* Process hierarchy */
    struct pcb  *parent;                    /* Pointer to parent process */
    queue_t     *children;                  /* Queue of child PCBs */
} PCB;

/* Allocate and initialize a new PCB with the given user page table */
PCB* CreatePCB(pte_t* user_page_table, UserContext* uctxt);

#endif /* PROCESS_H */
