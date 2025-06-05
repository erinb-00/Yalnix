#include "queue.h"
#include "process.h"
#include "sync_cvar.h"
#include "sync_lock.h"
#include <limits.h>

//=====================================================================
// Define cvar_t structure
//=====================================================================
typedef struct cvar {
    int cvar_id;
    queue_t* cvar_waiting_processes;
} cvar_t;

//=====================================================================
// Define CVAR_MAX and LOCK_MAX
//=====================================================================
static const int CVAR_MAX  = INT_MAX / 3 * 2;
static const int LOCK_MAX  = INT_MAX / 3; 

//=====================================================================
// Define next_cvar_id
//=====================================================================
static int next_cvar_id = LOCK_MAX + 1;

//=====================================================================
// Define CvarInit function
//=====================================================================
int CvarInit(int * cvar_idp){

    // check if the next_cvar_id is greater than CVAR_MAX
    if (next_cvar_id > CVAR_MAX) {
        TracePrintf(0, "CvarInit: Maximum number of condition variables reached\n");
        return ERROR;
    }

    // check if the cvar_idp is NULL
    if (cvar_idp == NULL){
        return ERROR;
    }

    // allocate memory for the new_cvar
    cvar_t* new_cvar = malloc(sizeof(cvar_t));
    if (new_cvar == NULL){
        TracePrintf(0, "CvarInit: Failed to allocate memory for cvar\n");
        return ERROR;
    }

    // add the new_cvar to the cvar_queue
    queue_add(cvar_queue, new_cvar);

    // set the cvar_id to the next_cvar_id
    new_cvar->cvar_id = next_cvar_id;

    // allocate memory for the cvar_waiting_processes
    new_cvar->cvar_waiting_processes = queue_new();

    // set the cvar_idp to the new_cvar_id
    *cvar_idp = new_cvar->cvar_id;
    next_cvar_id++;

    return 0;
}

//=====================================================================
// Define find_cvar_cb function
//=====================================================================
static void find_cvar_cb(void *item, void *ctx, void *ctx2) {
    cvar_t *p = (cvar_t *)item;
    int     target_id = *(int *)ctx;
    cvar_t **out_ptr = (cvar_t **)ctx2;

    if (p->cvar_id == target_id && *out_ptr == NULL) {
        *out_ptr = p;
    }
}

//=====================================================================
// Define CvarSignal function
//=====================================================================
int CvarSignal(int cvar_id){

    // check if the cvar_id is less than 1
    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return ERROR;
    }

    // find the cvar with the given cvar_id
    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);

    // check if the cvar is found
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return ERROR;
    }

    // check if the cvar_waiting_processes is not empty
    if (queue_size(found_cvar->cvar_waiting_processes) > 0){
        // get the next PCB from the cvar_waiting_processes
        PCB* next = queue_get(found_cvar->cvar_waiting_processes);

        // check if the next is NULL
        if (next == NULL){
            TracePrintf(0, "CvarInit: No process to signal even though size was greater than 0\n");
            return ERROR;
        }

        // set the state of the next to PCB_READY
        next->state = PCB_READY;

        // delete the next from the blocked_processes
        queue_delete_node(blocked_processes, next);

        // add the next to the ready_processes
        queue_add(ready_processes, next);

        // print the cvar signaled and process awakened
        TracePrintf(0, "CvarInit: Cvar signaled and process %d awakened\n", next->pid);
        return 0;
    }
    
    return ERROR;
}

//=====================================================================
// Define broadcast_cvar_cb function
//=====================================================================
static void broadcast_cvar_cb(void *item, void *ctx, void *ctx2) {
    PCB* p = (PCB *)item;
    p->state = PCB_READY;
    queue_delete_node(blocked_processes, p);
    queue_add(ready_processes, p);
}

//=====================================================================
// Define CvarBroadcast function
//=====================================================================
int CvarBroadcast(int cvar_id){

    // check if the cvar_id is less than 1
    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return ERROR;
    }

    // find the cvar with the given cvar_id
    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);

    // check if the cvar is found
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return ERROR;
    }

    // check if the cvar_waiting_processes is not empty
    if (queue_size(found_cvar->cvar_waiting_processes) > 0){
        // broadcast the cvar
        queue_iterate(found_cvar->cvar_waiting_processes, broadcast_cvar_cb, NULL, NULL);
        return 0;
    }

    return ERROR;
}

//=====================================================================
// Define CvarWait function
//=====================================================================
int CvarWait(int cvar_id, int lock_id){

    // check if the cvar_id or lock_id is less than 1
    if (cvar_id < 1 || lock_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar or lock id\n");
        return ERROR;
    }

    // release the lock
    Release(lock_id);

    // find the cvar with the given cvar_id
    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);

    // check if the cvar is found
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return ERROR;
    }

    // add the currentPCB to the cvar_waiting_processes
    queue_add(found_cvar->cvar_waiting_processes, currentPCB);

    // set the state of the currentPCB to PCB_BLOCKED
    currentPCB->state = PCB_BLOCKED;

    // add the currentPCB to the blocked_processes
    queue_add(blocked_processes, currentPCB);

    // get the previous PCB
    PCB* prev = currentPCB;

    // check if the prev is not the idlePCB and the state of the prev is PCB_READY
    if (prev != idlePCB && prev->state == PCB_READY) {
        // add the prev to the ready_processes
        queue_add(ready_processes, prev);
    }

    // get the next PCB
    PCB* next  = queue_get(ready_processes);

    // check if the next is NULL
    if (next == NULL) {
        // set the next to the idlePCB
        next = idlePCB;
    }

    // switch the kernel context
    KernelContextSwitch(KCSwitch, prev, next);

    // print the cvar waited and process awakened
    TracePrintf(0, "CvarInit: Cvar waited and process %d awakened\n", next->pid);

    // acquire the lock
    Acquire(lock_id);
    return 0;
}

//=====================================================================
// Define Reclaim_cvar function
//=====================================================================
int Reclaim_cvar(int cvar_id){

    // check if the cvar_id is less than 1
    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return ERROR;
    }

    // find the cvar with the given cvar_id
    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);

    // check if the cvar is found
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return ERROR;
    }

    // check if the cvar_waiting_processes is not empty
    if (queue_size(found_cvar->cvar_waiting_processes) > 0){
        TracePrintf(0, "CvarInit: Cvar is not empty\n");
        return ERROR;
    }

    // delete the cvar from the cvar_queue
    queue_delete_node(cvar_queue, found_cvar);

    // free the cvar_waiting_processes
    free(found_cvar->cvar_waiting_processes);

    // free the cvar
    free(found_cvar);

    // print the cvar reclaimed
    TracePrintf(0, "CvarInit: Cvar reclaimed\n");
    return 0;
}