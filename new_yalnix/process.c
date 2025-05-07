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
    blocked_queue = pcb_queue_create();
    zombie_queue = pcb_queue_create();
    waiting_parent_queue = pcb_queue_create();
}