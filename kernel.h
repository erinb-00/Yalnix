/* kernel.h - Main kernel header with core data structures */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "hardware.h"
#include "yalnix.h"

/* Forward declarations */
typedef struct pcb PCB;

/* Global kernel variables */
extern int vmem_enabled;           /* Flag indicating if virtual memory is enabled */
extern unsigned int kernel_brk;    /* Current break point for kernel heap */
extern unsigned int kernel_data_start; /* Start of kernel data segment */
extern unsigned int kernel_data_end;   /* End of kernel data segment */
extern unsigned int kernel_text_start; /* Start of kernel text segment */
extern unsigned int kernel_text_end;   /* End of kernel text segment */
extern unsigned int user_stack_limit;  /* Limit for user stack growth */

/* Page table pointers */
extern pte_t *kernel_page_table;   /* Page table for kernel region */
extern pte_t **process_page_tables; /* Array of page tables for user processes */

/* Kernel functions */
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);
int SetKernelBrk(void *addr);
void KernelInit(void);
void BootstrapInit(UserContext *uctxt);
void LoadInitProcess(UserContext *uctxt);
void EnableVirtualMemory(void);

/* Number of pages in the kernel stack */
#define KSTACK_NPAGES ((KERNEL_STACK_LIMIT >> PAGESHIFT) - (KERNEL_STACK_BASE >> PAGESHIFT))

typedef struct pcb {
    pte_t    *region1_pt;                         /* Region 1 page table for this process */
    int       pid;                                /* Process ID from helper_new_pid() */
    UserContext uctxt;                            /* Saved user-mode context */
    unsigned int kstack_pfn[KSTACK_NPAGES];       /* Physical frames for the kernel stack */
    struct PCB *next;                             /* For ready / free lists */
} PCB;

/* Globals in kernel.c */
static PCB *idlePCB;         /* The one and only idle process */
static PCB *currentPCB;      /* The process currently running (idle initially) */

#endif /* _KERNEL_H */
