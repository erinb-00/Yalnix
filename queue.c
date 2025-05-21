#include "process.h"
#include "ykernel.h"
#include "queue.h"

pcb_queue_t *pcb_queue_create(void) {
    pcb_queue_t *queue = malloc(sizeof(pcb_queue_t));
    if (queue == NULL) {
        TracePrintf(0, "pcb_queue_create Error: Unable to allocate memory for PCB queue\n");
        return NULL;
    }
    if (queue){
        queue->head = NULL;
        queue->tail = NULL;
        queue->size = 0;
    }
    return queue;
}

void pcb_enquque(pcb_queue_t *queue, pcb_t *pcb) {
    if (queue == NULL || pcb == NULL) {
        TracePrintf(0, "pcb_enquque Error: Invalid queue or PCB\n");
        Halt();
    }
    pcb->next = NULL;
    pcb->prev = queue->tail;
    if (pcb_queue_is_empty(queue)) {
        queue->head = pcb;
    } else {
        queue->tail->next = pcb;
    }
    queue->size++;
    queue->tail = pcb;
    TracePrintf(0, "Enqueued PCB %s (PID: %d) to queue\n", pcb->name, pcb->pid);
}

pcb_t *pcb_dequeue(pcb_queue_t *queue) {
    if (!queue->head) {
        TracePrintf(0, "pcb_dequeue Error: Queue is not initialized\n");
        return NULL;
    }
    pcb_t *pcb = queue->head;
    if (pcb== NULL) {
        TracePrintf(0, "pcb_dequeue Error: Queue is empty\n");
        return NULL;
    }
    queue->head = pcb->next;
    if (queue->head != NULL) {
        queue->tail = NULL;}
    queue->size--;
    TracePrintf(0, "Dequeued PCB %s (PID: %d) from queue\n", pcb->name, pcb->pid);
    return pcb;
}

int pcb_queue_is_empty(pcb_queue_t *queue) {
    return queue->size == 0;
}

void pcb_queue_remove(pcb_queue_t *queue, pcb_t *pcb) {
    if (queue == NULL || pcb == NULL) {
        TracePrintf(0, "pcb_queue_remove Error: Invalid queue or PCB\n");
        return;
    }
    if (pcb->prev != NULL) {
        pcb->prev->next = pcb->next;
    } else {
        queue->head = pcb->next;
    }
    if (pcb->next != NULL) {
        pcb->next->prev = pcb->prev;
    } else {
        queue->tail = pcb->prev;
    }
    queue->size--;
    TracePrintf(0, "Removed PCB %s (PID: %d) from queue\n", pcb->name, pcb->pid);
}

int pcb_in_queue(pcb_queue_t *queue, pcb_t *pcb) {
    if (queue == NULL || pcb == NULL) {
        TracePrintf(0, "pcb_in_queue Error: Invalid queue or PCB\n");
        return 0;
    }
    for (pcb_t *current = queue->head; current != NULL; current = current->next) {
        if (current == pcb) {
            return 1;
        }
    }
    return 0;
}
