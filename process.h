#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>
#include "yalnix.h"    // for pte_t, UserContext, PAGESHIFT, KERNEL_STACK_BASE/LIMIT
#include "hardware.h"
#include "ykernel.h"

/* Number of pages in the kernel stack */
#define KSTACK_NPAGES \
    ((KERNEL_STACK_LIMIT >> PAGESHIFT) - (KERNEL_STACK_BASE >> PAGESHIFT))

typedef enum pcb_state {
    PCB_READY,
    PCB_RUNNING,
    PCB_BLOCKED,
    PCB_ZOMBIE,
    PCB_ORPHANED
} pcb_state_t;

typedef struct pcb {
    pte_t       *region1_pt;                /* Region 1 page table */
    int          pid;                       /* From helper_new_pid() */
    UserContext  uctxt;                     /* Saved user-mode context */
    unsigned int kstack_pfn[KSTACK_NPAGES]; /* PFNs for the kernel stack */
    struct pcb  *next;                      /* Ready/free list linkage */
    void* brk;
    KernelContext kctxt;
    pcb_state_t state;               /* Process state */
} PCB;

extern pcb_t *idle_pcb;                       // The idle process
extern pcb_queue_t *ready_processes;          // Queue of processes ready to run
extern pcb_queue_t *blocked_processes;        // Queue of blocked processes
extern pcb_queue_t *zombie_processes;        // Queue of zombie processes
extern pcb_queue_t *waiting_parent_processes; // Queue of processes waiting for their children

static pcb_t *current_process; // Currently running process

/* Allocate and initialize a new PCB with the given user page table */
PCB* CreatePCB(pte_t* user_page_table);

pcb_t *GetCurrentProcess(void);
void SetCurrentProcess(pcb_t *pcb);
#endif /* PROCESS_H */

