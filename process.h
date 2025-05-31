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

//============================================
// CP4:- Track Status for round-robin
//============================================
typedef enum pcb_state {
    PCB_READY,
    PCB_RUNNING,
    PCB_BLOCKED,
    PCB_ZOMBIE,
    PCB_ORPHANED
} pcb_state_t;
  
//==========================================================================
// CP2:- PCB Structure
//==========================================================================
typedef struct pcb {
    pte_t       *region1_pt;                /* Region 1 page table */
    int          pid;                       /* From helper_new_pid() */
    UserContext  uctxt;                     /* Saved user-mode context */
    unsigned int kstack_pfn[KSTACK_NPAGES]; /* PFNs for the kernel stack */
    struct pcb  *next;                      /* Ready/free list linkage */
    void        *brk;                       /* Current break (heap) */
    KernelContext kctxt;                    /* Saved kernel-mode context */
    int         exit_status;                /* Exit status for the process */
    int         num_delay;
    struct pcb  *parent;                    /* Pointer to parent process */
    queue_t     *children;                  /* Queue of child PCBs */
    pcb_state_t state;               /* Process state */
    char* read_buffer;
    int read_buffer_size;
    const char* write_buffer;
    int write_buffer_size;
} PCB;

//============================================
// CP4:- Tracking queues for round-robin
//============================================
extern queue_t *ready_processes;          // Queue of processes ready to run
extern queue_t *blocked_processes;        // Queue of blocked processes
extern queue_t *zombie_processes;        // Queue of zombie processes
extern queue_t *waiting_parent_processes; // Queue of processes waiting for their children
extern queue_t *pipes_queue;              // Queue of pipes
extern queue_t *locks_queue;              // Queue of locks
extern queue_t *cvar_queue;               // Queue of condition variables

//==========================================================================
// CP2:- Allocate and initialize a new PCB with the given user page table
//==========================================================================
PCB* CreatePCB(pte_t* user_page_table, UserContext* uctxt);

//==============================================
// CP4:- Initialize queues to track processes
//==============================================
void initQueues(void);

#endif /* PROCESS_H */
                         