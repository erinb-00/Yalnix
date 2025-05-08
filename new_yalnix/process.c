#include "process.h"
#include "kernel.h"
#include "hardware.h"
#include "yalnix.h"
#include "queue.h"
#include "ykernel.h"

// Global Variables
pcb_queue_t *ready_queue = NULL;
pcb_queue_t *blocked_queue = NULL;
pcb_queue_t *zombie_queue = NULL;
pcb_queue_t *waiting_parent_queue = NULL;
pcb_t *idle_pcb = NULL;

void InitProcessQueue(void) {
    ready_queue = pcb_queue_create();
    if (ready_queue == NULL) {
        TracePrintf(0, "InitProcessQueue Error: Unable to create ready queue\n");
        Halt();
    }
    blocked_queue = pcb_queue_create();
    if (blocked_queue == NULL) {
        TracePrintf(0, "InitProcessQueue Error: Unable to create blocked queue\n");
        Halt();
    }
    zombie_queue = pcb_queue_create();
    if (zombie_queue == NULL) {
        TracePrintf(0, "InitProcessQueue Error: Unable to create zombie queue\n");
        Halt();
    }
    waiting_parent_queue = pcb_queue_create();
    if (waiting_parent_queue == NULL) {
        TracePrintf(0, "InitProcessQueue Error: Unable to create waiting parent queue\n");
        Halt();
    }
}

pcb_t *GetCurrentProcess(void) {
    return current_pcb;
}

void SetCurrentProcess(pcb_t *pcb) {
    current_pcb = pcb;
    pcb->state = PCB_RUNNING;
}

pcb_t *CreatePCB(char *name) {
    pcb_t *pcb = (pcb_t *)malloc(sizeof(pcb_t));
    if (pcb == NULL) {
        TracePrintf(0, "CreatePCB Error: Unable to allocate memory for PCB\n");
        return NULL;
    }
    pcb->state = PCB_READY;
    pcb->page_table = (pte_t *)calloc(NUM_PAGES_REGION1, sizeof(pte_t));
    if (pcb->page_table == NULL) {
        TracePrintf(0, "CreatePCB Error: Unable to allocate memory for page table\n");
        free(pcb->name);
        free(pcb);
        return NULL;
    }
    pcb->kernel_stack = NULL;
    pcb->brk = NULL;
    pcb->uctxt = (UserContext){0};
    pcb->kctxt = (KernelContext){0};
    pcb->next = NULL;
    pcb->prev = NULL;
    pcb->parent = NULL;
    pcb->delay = -1;
    pcb->exit_code = 0;
    pcb->children = pcb_queue_create();
    if(pcb->children == NULL) {
        TracePrintf(0, "CreatePCB Error: Unable to create children queue\n");
        free(pcb->page_table);
        free(pcb->name);
        free(pcb);
        return NULL;
    }
    pcb->name = strdup(name);
    if (pcb->name == NULL) {
        TracePrintf(0, "CreatePCB Error: Unable to allocate memory for process name\n");
        free(pcb);
        return NULL;
    }
    pcb->pid = helper_new_pid(pcb->page_table);

    pcb->tty_read_buffer = NULL;
    pcb->tty_read_buffer_size = 0;
    pcb->tty_write_buffer = NULL;
    pcb->tty_write_buffer_size = 0;
    pcb->kernel_read_buffer = NULL;
    pcb->kernel_read_buffer_size = 0;
    return pcb;
}

void FreePCB(pcb_t *pcb) {
    if (pcb == NULL) {
        return;
    }
    free(pcb->name);
    free(pcb->page_table);
    free(pcb->kernel_stack);
    free(pcb->children);
    free(pcb);
}
void UpdateDelayedProcess(pcb_t *pcb, int delay) {
    if (pcb == NULL) {
        return;
    }
    pcb->delay = delay;
}

void PrintPageTable(pcb_t *pcb) {
    if (pcb == NULL) {
        return;
    }
    TracePrintf(0, "Page Table for Process %s (PID: %d):\n", pcb->name, pcb->pid);
    for (int i = 0; i < NUM_PAGES_REGION1; i++) {
        TracePrintf(0, "Page %d: %p\n", i, pcb->page_table[i]);
    }
}

void CopyPageTable(pcb_t *src, pcb_t *dest) {
    if (src == NULL || dest == NULL) {
        return;
    }
    memcpy(dest->page_table, src->page_table, sizeof(pte_t) * NUM_PAGES_REGION1);
}   