#include <stdlib.h>
#include "process.h"
#include "ykernel.h"
#include "queue.h"
#include "process.h"

//=====================================================================
// Define create_node function
//=====================================================================
static queue_node_t* create_node(void* item) {
    queue_node_t* node = malloc(sizeof(queue_node_t));
    if (!node) return NULL;
    node->item = item;
    node->next = node->prev = NULL;
    return node;
}

//=====================================================================
// Define queue_new function
//=====================================================================
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

//=====================================================================
// Define queue_size function
//=====================================================================
int queue_size(queue_t* queue){
    return queue->size;
}

//=====================================================================
// Define queue_add function
//=====================================================================
void queue_add(queue_t* queue, void* item) {

    if (queue == NULL || item == NULL) {
        TracePrintf(0, "queue_add failed: queue or PCB was NULL\n");
        Halt();
    }

    queue_node_t* node = create_node(item);
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

//=====================================================================
// Define queue_get function
//=====================================================================
void* queue_get(queue_t* queue) {
    if (queue == NULL){
        return NULL;
    }
    if (queue->head == NULL) {
        return NULL;
    }
    queue_node_t* node = queue->head;
    void* result = node->item;
    queue->head = node->next;
    if (queue->head)
        queue->head->prev = NULL;
    else
        queue->tail = NULL;
    free(node);
    queue->size--;
    return result;
}

//=====================================================================
// Define queue_is_empty function
//=====================================================================
int queue_is_empty(queue_t* queue) {
    return (queue && queue->size == 0);
}

//=====================================================================
// Define queue_delete_node function
//=====================================================================
void queue_delete_node(queue_t* queue, void* item) {
    if (!queue) return;
    queue_node_t* current = queue->head;
    while (current) {
        if (current->item == item) {
            queue_node_t* prev = current->prev;
            queue_node_t* next = current->next;
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

//=====================================================================
// Define queue_find function
//=====================================================================
int queue_find(queue_t* queue, void* item) {
    int idx = 0;
    queue_node_t* current = queue->head;
    while (current) {
        if (current->item == item) return idx;
        current = current->next;
        idx++;
    }
    return -1;
}

//=====================================================================
// Define queue_delete function
//=====================================================================
void queue_delete(queue_t* queue) {
    if (!queue) return;
    queue_node_t* current = queue->head;
    while (current) {
        queue_node_t* next = current->next;
        free(current);
        current = next;
    }
    free(queue);
}

//=====================================================================
// Define queue_iterate function
//=====================================================================
void queue_iterate(queue_t *queue, queue_callback_t cb, void *ctx, void* ctx2) {
    if (!queue || !cb) return;
    queue_node_t *cur = queue->head;
    while (cur) {
        cb(cur->item, ctx, ctx2);
        cur = cur->next;
    }
}