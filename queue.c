#include <stdlib.h>
#include "process.h"
#include "ykernel.h"
#include "queue.h"
#include "process.h"

// Internal node structure for doubly-linked list
struct queue_node {
    PCB* pcb;
    struct queue_node* next;
    struct queue_node* prev;
};

struct queue_t {
    struct queue_node* head;
    struct queue_node* tail;
    int size;
};

// Creates a new node for the queue (static/internal)
static struct queue_node* create_node(PCB* pcb) {
    struct queue_node* node = malloc(sizeof(struct queue_node));
    if (!node) return NULL;
    node->pcb = pcb;
    node->next = node->prev = NULL;
    return node;
}

queue_t* queue_new(void) {

    queue_t* queue = malloc(sizeof(queue_t));
    if (queue == NULL) {
        TracePrintf(0, "Unable to allocate memory for queue\n");
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

void queue_add(queue_t* queue, PCB* pcb) {

    if (queue == NULL || pcb == NULL) {
        TracePrintf(0, "queue_add failed: queue or PCB was NULL\n");
        Halt();
    }

    struct queue_node* node = create_node(pcb);
    if (node == NULL){
        return; // Allocation failed
    }

    if (queue->tail) {
        queue->tail->next = node;
        node->prev = queue->tail;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    queue->size++;
}

PCB* queue_get(queue_t* queue) {
    if (queue == NULL){
        TracePrintf(0, "queue_get Error: queue is NULL\n");
        return NULL;
    }
    if (queue->head == NULL) {
        TracePrintf(0, "queue_get Error: queue head is NULL\n");
        return NULL;
    }
    struct queue_node* node = queue->head;
    PCB* result = node->pcb;
    queue->head = node->next;
    if (queue->head)
        queue->head->prev = NULL;
    else
        queue->tail = NULL;
    free(node);
    queue->size--;
    return result;
}

int queue_is_empty(queue_t* queue) {
    return (queue && queue->size == 0);
}

void queue_delete_node(queue_t* queue, PCB* pcb) {
    if (!queue) return;
    struct queue_node* current = queue->head;
    while (current) {
        if (current->pcb == pcb) {
            struct queue_node* prev = current->prev;
            struct queue_node* next = current->next;
            if (prev) prev->next = next;
            else queue->head = next;
            if (next) next->prev = prev;
            else queue->tail = prev;
            free(current);
            queue->size--;
            return;
        }
        current = current->next;
    }
}

int queue_find(queue_t* queue, PCB* pcb) {
    int idx = 0;
    struct queue_node* current = queue->head;
    while (current) {
        if (current->pcb == pcb) return idx;
        current = current->next;
        idx++;
    }
    return -1;
}

void queue_delete(queue_t* queue) {
    if (!queue) return;
    struct queue_node* current = queue->head;
    while (current) {
        struct queue_node* next = current->next;
        free(current);
        current = next;
    }
    free(queue);
}
