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
int get_free_frame();
void free_frame_number(int index);

extern PCB *idlePCB;         /* The one and only idle process */
extern PCB *initPCB;         /* The user init process */
extern PCB *currentPCB;

KernelContext* KCSwitch(KernelContext *kc_in, void *curr_pcb_p,void *next_pcb_p);
KernelContext* KCCopy(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);
int LoadProgram(char *name, char *args[], PCB* proc);

#endif /* _KERNEL_H */
