/*
 * process.c - Process management implementation
 */

 #include <stdlib.h>
 #include "process.h"
 #include "hardware.h"
 #include "yalnix.h"
 #include "memory.h"
 
 /* Global process management variables */
 PCB *current_process = NULL;                  /* Currently running process */
 ProcessQueue ready_queue = {NULL, NULL};      /* Queue of ready processes */
 ProcessQueue blocked_queue = {NULL, NULL};    /* Queue of blocked processes */
 ProcessQueue zombie_queue = {NULL, NULL};     /* Queue of zombie processes */
 PCB *process_table[MAX_PROCS];            /* Array of all processes indexed by PID */
 int next_pid = 0;                             /* Next available PID */
 
 /* Local function prototypes */
 static PCB* CreatePCB(void);
 static void EnqueueProcess(ProcessQueue *queue, PCB *proc);
 static PCB* DequeueProcess(ProcessQueue *queue);
 static void FreeProcessResources(PCB *proc);
 
 /*
  * Initialize the process management subsystem
  */
 void ProcessInit(void) {
     int i;
     
     /* Initialize the process table */
     for (i = 0; i < MAX_PROCS; i++) {
         process_table[i] = NULL;
     }
     
     /* Create the initial process (kernel process) */
     PCB *init_proc = CreatePCB();
     if (init_proc == NULL) {
         /* Fatal error - can't create initial process */
         KernelPanic("Failed to create initial process");
         return;
     }
     
     /* Set up the initial process */
     init_proc->pid = next_pid++;
     init_proc->ppid = -1;  /* No parent */
     init_proc->state = PROCESS_RUNNING;
     
     /* Allocate and set up the kernel page table */
     init_proc->page_table = AllocatePageTable();
     if (init_proc->page_table == NULL) {
         KernelPanic("Failed to allocate page table for initial process");
         free(init_proc);
         return;
     }
     
     /* Initialize kernel stack and user stack information */
     init_proc->kernel_stack = AllocateKernelStack();
     init_proc->user_brk = VMEM_0_LIMIT;  /* Initial program break */
     init_proc->user_stack_base = VMEM_1_BASE;
     init_proc->user_stack_limit = VMEM_1_LIMIT;
     
     /* Add the initial process to the process table */
     process_table[init_proc->pid] = init_proc;
     
     /* Set the current process */
     current_process = init_proc;
 }
 
 /*
  * Create a new process (fork)
  * Returns: PID of the child process in the parent, 0 in the child, or -1 on error
  */
 int ProcessFork(void) {
     if (next_pid >= MAX_PROCS) {
         /* Too many processes */
         return -1;
     }
     
     /* Create a new PCB for the child process */
     PCB *child = CreatePCB();
     if (child == NULL) {
         return -1;
     }
     
     /* Set up the child process */
     child->pid = next_pid++;
     child->ppid = current_process->pid;
     child->state = PROCESS_READY;
     
     /* Copy the parent's user context to the child */
     child->uctxt = current_process->uctxt;
     
     /* Clone the parent's address space */
     child->page_table = ClonePageTable(current_process->page_table);
     if (child->page_table == NULL) {
         free(child);
         return -1;
     }
     
     /* Set up child's stacks and heap */
     child->kernel_stack = AllocateKernelStack();
     child->user_brk = current_process->user_brk;
     child->user_stack_base = current_process->user_stack_base;
     child->user_stack_limit = current_process->user_stack_limit;
     
     /* Set return values in user contexts */
     child->uctxt.regs[0] = 0;  /* Child gets 0 */
     
     /* Add the child to the process table */
     process_table[child->pid] = child;
     
     /* Add the child to the ready queue */
     EnqueueProcess(&ready_queue, child);
     
     /* Return the child's PID to the parent */
     return child->pid;
 }
 
 /*
  * Exit the current process
  */
 void ProcessExit(int status) {
     /* Save the exit status for the parent process */
     current_process->exit_status = status;
     
     /* Change the state to zombie */
     current_process->state = PROCESS_ZOMBIE;
     
     /* Free process resources except PCB (kept for parent to retrieve exit status) */
     FreeUserPages(current_process->page_table);
     FreeKernelStack(current_process->kernel_stack);
     
     /* Add to zombie queue */
     EnqueueProcess(&zombie_queue, current_process);
     
     /* Schedule the next process */
     ProcessSchedule();
 }
 
 /*
  * Wait for a child process to terminate
  * Returns: PID of the terminated child, or -1 if no children
  */
 int ProcessWait(int *status) {
     PCB *zombie;
     PCB *prev;
     int pid;
     
     /* Check the zombie queue for children of the current process */
     zombie = zombie_queue.head;
     prev = NULL;
     
     while (zombie != NULL) {
         if (zombie->ppid == current_process->pid) {
             /* Found a zombie child */
             
             /* Remove from zombie queue */
             if (prev == NULL) {
                 zombie_queue.head = zombie->next;
             } else {
                 prev->next = zombie->next;
             }
             
             if (zombie_queue.tail == zombie) {
                 zombie_queue.tail = prev;
             }
             
             /* Get the terminated child's information */
             pid = zombie->pid;
             if (status != NULL) {
                 *status = zombie->exit_status;
             }
             
             /* Clean up the zombie PCB */
             process_table[pid] = NULL;
             free(zombie);
             
             return pid;
         }
         
         prev = zombie;
         zombie = zombie->next;
     }
     
     /* If no zombie children found, block if there are still running children */
     for (pid = 0; pid < MAX_PROCS; pid++) {
         if (process_table[pid] != NULL && process_table[pid]->ppid == current_process->pid) {
             /* There's still at least one child running, so block */
             current_process->state = PROCESS_BLOCKED;
             EnqueueProcess(&blocked_queue, current_process);
             ProcessSchedule();
             
             /* When we wake up, try again */
             return ProcessWait(status);
         }
     }
     
     /* No children at all */
     return -1;
 }
 
 /*
  * Run the next ready process
  */
 void ProcessSchedule(void) {
     PCB *next_process;
     
     /* Get the next process from the ready queue */
     next_process = DequeueProcess(&ready_queue);
     
     /* If there's no ready process, halt the system */
     if (next_process == NULL) {
         TracePrintf(0, "No ready processes to run. Halting.\n");
         Halt();
     }
     
     /* Set the state of the new process to running */
     next_process->state = PROCESS_RUNNING;
     
     /* Switch to the new process context */
     PCB *old_process = current_process;
     current_process = next_process;
     
     /* If the old process was running (not terminated or blocked), add it to the ready queue */
     if (old_process != NULL && old_process->state == PROCESS_RUNNING) {
         old_process->state = PROCESS_READY;
         EnqueueProcess(&ready_queue, old_process);
     }
     
     /* Switch page tables */
     //WriteRegister(REG_PTR0, (RCS421RegVal)current_process->page_table);
     WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
     
     /* Restore the user context */
     ContextSwitch(KernelContextSwitch, &current_process->uctxt, &old_process->uctxt);
 }
 
 /*
  * Unblock all processes that were blocked waiting for a child to exit
  */
 void ProcessWakeParent(int ppid) {
     PCB *proc = blocked_queue.head;
     PCB *prev = NULL;
     PCB *next;
     
     while (proc != NULL) {
         next = proc->next;
         
         /* If this is the parent waiting for a child exit */
         if (proc->pid == ppid) {
             /* Remove from blocked queue */
             if (prev == NULL) {
                 blocked_queue.head = proc->next;
             } else {
                 prev->next = proc->next;
             }
             
             if (blocked_queue.tail == proc) {
                 blocked_queue.tail = prev;
             }
             
             /* Mark as ready and add to ready queue */
             proc->state = PROCESS_READY;
             EnqueueProcess(&ready_queue, proc);
         } else {
             prev = proc;
         }
         
         proc = next;
     }
 }
 
 /*
  * Create a new empty PCB
  */
 static PCB* CreatePCB(void) {
     PCB *pcb = (PCB *)malloc(sizeof(PCB));
     if (pcb == NULL) {
         return NULL;
     }
     
     /* Initialize fields */
     pcb->pid = -1;
     pcb->ppid = -1;
     pcb->state = PROCESS_INVALID;
     pcb->page_table = NULL;
     pcb->kernel_stack = 0;
     pcb->user_brk = 0;
     pcb->user_stack_base = 0;
     pcb->user_stack_limit = 0;
     pcb->exit_status = 0;
     pcb->next = NULL;
     
     return pcb;
 }
 
 /*
  * Add a process to the end of a process queue
  */
 static void EnqueueProcess(ProcessQueue *queue, PCB *proc) {
     proc->next = NULL;
     
     if (queue->tail == NULL) {
         /* Empty queue */
         queue->head = proc;
         queue->tail = proc;
     } else {
         /* Add to end of queue */
         queue->tail->next = proc;
         queue->tail = proc;
     }
 }
 
 /*
  * Remove and return the first process from a process queue
  * Returns NULL if the queue is empty
  */
 static PCB* DequeueProcess(ProcessQueue *queue) {
     PCB *proc;
     
     if (queue->head == NULL) {
         return NULL;
     }
     
     proc = queue->head;
     queue->head = proc->next;
     
     if (queue->head == NULL) {
         queue->tail = NULL;
     }
     
     proc->next = NULL;
     return proc;
 }
 
 /*
  * Free resources associated with a process
  */
 static void FreeProcessResources(PCB *proc) {
     if (proc == NULL) {
         return;
     }
     
     /* Free page table entries */
     if (proc->page_table != NULL) {
         FreePageTable(proc->page_table);
     }
     
     /* Free kernel stack */
     if (proc->kernel_stack != 0) {
         FreeKernelStack(proc->kernel_stack);
     }
 }