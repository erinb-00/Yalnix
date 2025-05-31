// queue.h
#ifndef QUEUE_H
#define QUEUE_H

typedef struct pcb PCB;
typedef struct queue_t queue_t;

queue_t* queue_new(void);
int queue_size(queue_t* queue);
void queue_add(queue_t* queue, void* item);
void* queue_get(queue_t* queue);
int queue_is_empty(queue_t* queue);
void queue_delete_node(queue_t* queue, void* item);
int queue_find(queue_t* queue, void* item);
void queue_delete(queue_t* queue);
typedef void (*queue_callback_t)(void* item, void *ctx, void* ctx2);
void queue_iterate(queue_t *queue, queue_callback_t cb, void *ctx, void* ctx2);

#endif // QUEUE_H
