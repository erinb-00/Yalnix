#ifndef process_h
#define process_h

#include "hardware.h"
#include "yalnix.h"
#include "queue.h"

typedef enum pcb_state {
    PCB_RUNNING,
    PCB_READY,
    PCB_BLOCKED,
    PCB_ZOMBIE,
    PCB_INVALID,
} pcb_state_t;

typedef struct pcb {
    char *name; // Process name
    int pid; // Process ID
    pcb_state_t state; // Process state

    pte_t *page_table; // Pointer to the page table
    pte_t *kernel_stack; // Pointer to the kernel stack
    void *brk; // Pointer to the current break

    UserContext uctxt; // User context
    KernelContext kctxt; // Kernel context
    struct PCB *next; // Pointer to the next PCB in the queue

    pcb_t *next; // Pointer to the next PCB in the queue
    pcb_t *prev; // Pointer to the previous PCB in the queue
    pcb_t *parent; // Pointer to the parent PCB
    pcb_queue_t *children; // Pointer to the children queue

    char *kernel_read_buffer; // Kernel read buffer
    int kernel_read_buffer_size; // Size of the kernel read buffer  

    int delay; // Delay time for the process
    int exit_code; // Exit code for the process

    void *tty_read_buffer; // TTY read buffer
    int tty_read_buffer_size; // Size of the TTY read buffer
    void *tty_write_buffer; // TTY write buffer
    int tty_write_buffer_size; // Size of the TTY write buffer
} pcb_t;

extern pcb_t *idle_pcb; // Pointer to the idle PCB
extern pcb_queue_t *ready_queue; // Pointer to the ready queue
extern pcb_queue_t *blocked_queue; // Pointer to the blocked queue
extern pcb_queue_t *zombie_queue; // Pointer to the zombie queue
extern pcb_queue_t *waiting_parent_queue; // Pointer to the free queue

static pcb_t *current_pcb; // Pointer to the current PCB

/**
 * @brief CreatePCB: Creates a new PCB and initializes it
 * 
 * @param name  : The name of the process
 * @return pcb_t*   : Pointer to the created PCB
 */
pcb_t *CreatePCB(char *name);

/**
 * @brief GetCurrentProcess: Get the Current Process
 * @return pcb_t* 
 */
pcb_t *GetCurrentProcess(void);

/**
 * @brief InitProcessQueue: Initializes the process queues
 */
void InitProcessQueue(void);

/**
 * @brief SetCurrentProcess: Set the Current Process to given PCB
 * @param pcb   : Pointer to the PCB to be set as current
 */
void SetCurrentProcess(pcb_t *pcb);

/**
 * @brief DestroyPCB: Destroys the given PCB
 * @param pcb   : Pointer to the PCB to be freed
 */
void DestroyPCB(pcb_t *pcb);

/**
 * @brief UpdateDelayedProcesses: Updates the delayed processes
 */
void UpdateDelayedProcesses(void);

/**
 * @brief PrintPageTable: Prints the page table of the given PCB
 * @param pcb   : Pointer to the PCB whose page table is to be printed
 */
void PrintPageTable(pcb_t *pcb);

/**
 * @brief CopyPageTable: Copies the page table from parent to child
 * @param parent: Pointer to the parent PCB
 * @param child  : Pointer to the child PCB
 */
void CopyPageTable(pcb_t *parent, pcb_t *child);
#endif // process_h