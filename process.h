/* process.h - Process management data structures and functions */

#ifndef _PROCESS_H
#define _PROCESS_H

#include "hardware.h"
#include "yalnix.h"

/* Process states */
typedef enum {
    PROCESS_RUNNING,
    PROCESS_READY,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE,
    PROCESS_INVALID
} ProcessState;

/* Process Control Block structure */
typedef struct pcb {
    int pid;                  /* Process ID */
    int ppid;                 /* Parent process ID */
    ProcessState state;       /* Current state */
    UserContext uctxt;        /* User context (registers, etc.) */
    pte_t *page_table;        /* Page table for this process */
    unsigned int kernel_stack; /* Kernel stack for this process */
    unsigned int user_brk;    /* Current user program break */
    unsigned int user_stack_base;  /* Base of user stack */
    unsigned int user_stack_limit; /* Current limit of user stack growth */
    int exit_status;          /* Exit status for zombie processes */
    struct pcb *next;         /* Next process in queue */
} PCB;

/* Queue of processes */
typedef struct {
    PCB *head;
    PCB *tail;
} ProcessQueue;

/* Global process management variables */
extern PCB *current_process;     /* Currently running process */
extern ProcessQueue ready_queue; /* Queue of ready processes */
extern ProcessQueue blocked_queue; /* Queue of blocked processes */
extern ProcessQueue zombie_queue;  /* Queue of zombie processes */
extern PCB *process_table[];      /* Array of all processes indexed by PID */
extern int next_pid;              /* Next available PID */

/* Process management functions */
void ProcessInit(void);

#endif /* _PROCESS_H */