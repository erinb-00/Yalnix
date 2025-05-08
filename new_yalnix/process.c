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

void DestroyPCB(pcb_t *pcb) {
    if (pcb->children != NULL) {
        for (pcb_t *child = pcb->children->head; child != NULL; child = child->next) {
            child->parent = NULL;
            child->state = PCB_ORPHAN;
        }
        free(pcb->children);
    }
    if(pcb->next != NULL) {
        pcb->next->prev = pcb->prev;
    }
    if(pcb->prev != NULL) {
        pcb->prev->next = pcb->next;
    }
    for (int i = 0; i < NUM_PAGES_REGION1; i++) {
        if (pcb->page_table[i].valid) {
            freeFrame(pcb->page_table[i].pfn);
            TracePrintf(0, "Freeing page %d from process %s (PID: %d)\n", i, pcb->name, pcb->pid);
            pcb->page_table[i].valid = 0;
        }
    }

    free(pcb->name);
    free(pcb->page_table);
    free(pcb->kernel_stack);
    free(pcb->children);
    free(pcb);
}
void UpdateDelayedProcess(pcb_t *pcb, int delay) {
    TracePrintf(0, "Updating delay for process %s (PID: %d) to %d\n", pcb->name, pcb->pid, delay);
    if (!pcb_queue_is_empty(blocked_queue)) {
         for (pcb_t *blocked_pcb = blocked_queue->head; blocked_pcb != NULL; blocked_pcb = blocked_pcb->next) {
            if (blocked_pcb -> delay == -1) {
                continue;
            }
            blocked_pcb->delay--;
            TracePrintf(0, "Decrementing delay for process %s (PID: %d) to %d\n", blocked_pcb->name, blocked_pcb->pid, blocked_pcb->delay);
            if (blocked_pcb->delay == 0) {
                blocked_pcb->state = PCB_READY;
                pcb_queue_remove(blocked_queue, blocked_pcb);
                pcb_enquque(ready_queue, blocked_pcb);
            }
        }
    }
}


void PrintPageTable(pcb_t *pcb) {
    for (int i = 0; i < NUM_PAGES_REGION1; i++) {
        TracePrintf(0, "Page %d: Frame %d, Valid %d\n", i, pcb->page_table[i].pfn, pcb->page_table[i].valid);
    }
}

void CopyPageTable(pcb_t *parent, pcb_t *child) {
    pte_t *parent_pt = parent->page_table;
    pte_t *child_pt = child->page_table;

    for (int i = 0; i < NUM_PAGES_REGION1; i++) {
        if (parent_pt[i].valid) {
            int child_pfn = getFreeFrame();
            child_pt[i].pfn = child_pfn;
            unsigned int parent_addr = (i + NUM_PAGES_REGION1) << PAGESHIFT;
            MapScratch(child_pfn, parent_addr);
            memcpy((void*)SCRATCH_ADDR, (void*)parent_addr, PAGESIZE);
            UnmapScratch(parent_addr);
            child_pt[i].valid = 1;
            child_pt[i].prot = parent_pt[i].prot;
            child_pt[i].pfn = getFreeFrame();
        }
    }
}