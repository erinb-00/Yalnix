#ifndef queue_h
#define queue_h

typedef struct pcb pcb_t;

typedef struct pcb_queue {
    pcb_t *head; // Pointer to the head of the queue
    pcb_t *tail; // Pointer to the tail of the queue
    int size; // Size of the queue
} pcb_queue_t;

/**
 * @brief pcb_queue_create: Creates a new PCB queue and initializes it
 * @return pcb_queue_t*     : Pointer to the created PCB queue
 */
pcb_queue_t *pcb_queue_create(void);

/**
 * @brief pcb_queue_enqueue: Adds a PCB to the end of the queue
 * @param queue: Pointer to the PCB queue
 * @param pcb: Pointer to the PCB to be added
 */
void pcb_enquque(pcb_queue_t *queue, pcb_t *pcb);

/**
 * @brief pcb_queue_dequeue: Removes a PCB from the front of the queue
 * @param queue: Pointer to the PCB queue
 * @return pcb_t* : Pointer to the removed PCB
 */
pcb_t *pcb_dequeue(pcb_queue_t *queue);

/**
 * @brief pcb_queue_is_empty: Checks if the queue is empty
 * @param queue: Pointer to the PCB queue
 * @return int : 1 if the queue is empty, 0 otherwise
 */
pcb_t *pcb_queue_is_empty(pcb_queue_t *queue);

/**
 * @brief pcb_queue_remove: Removes a PCB from the queue
 * @param queue: Pointer to the PCB queue
 * @param pcb: Pointer to the PCB to be removed
 */
void pcb_queue_remove(pcb_queue_t *queue, pcb_t *pcb);

/**
 * @brief pcb_queue_peek: Returns the PCB at the front of the queue without removing it
 * @param queue: Pointer to the PCB queue
 * @return pcb_t* : Pointer to the PCB at the front of the queue
 */
int pcb_in_queue(pcb_queue_t *queue, pcb_t *pcb);

/**
 * @brief pcb_queue_destroy: Destroys the PCB queue and frees its memory
 * @param queue: Pointer to the PCB queue
 */
void pcb_queue_destroy(pcb_queue_t *queue);

#endif // queue_h
