// queue.h
#ifndef QUEUE_H
#define QUEUE_H

typedef struct pcb PCB;
typedef struct queue_t queue_t;

queue_t* queue_new(void);
void queue_add(queue_t* queue, PCB* pcb);
PCB* queue_get(queue_t* queue);
int queue_is_empty(queue_t* queue);
void queue_delete_node(queue_t* queue, PCB* pcb);
int queue_find(queue_t* queue, PCB* pcb);
void queue_delete(queue_t* queue);
typedef void (*queue_callback_t)(PCB *pcb, void *ctx);
void queue_iterate(queue_t *queue, queue_callback_t cb, void *ctx);

#endif // QUEUE_H
