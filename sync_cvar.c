#include "queue.h"
#include "process.h"
#include "sync_cvar.h"
#include "sync_lock.h"
#include <limits.h>

typedef struct cvar {
    int cvar_id;
    queue_t* cvar_waiting_processes;
} cvar_t;

static const int CVAR_MAX  = INT_MAX / 3 * 2;
static const int LOCK_MAX  = INT_MAX / 3; 
static int next_cvar_id = LOCK_MAX + 1;

int CvarInit(int * cvar_idp){

    if (next_cvar_id > CVAR_MAX) {
        TracePrintf(0, "CvarInit: Maximum number of condition variables reached\n");
        return -1;
    }

    if (cvar_idp == NULL){
        return -1;
    }

    cvar_t* new_cvar = malloc(sizeof(cvar_t));
    if (new_cvar == NULL){
        TracePrintf(0, "CvarInit: Failed to allocate memory for cvar\n");
        return -1;
    }

    queue_add(cvar_queue, new_cvar);
    new_cvar->cvar_id = next_cvar_id;
    new_cvar->cvar_waiting_processes = queue_new();
    *cvar_idp = new_cvar->cvar_id;
    next_cvar_id++;

    return 0;
}

static void find_cvar_cb(void *item, void *ctx, void *ctx2) {
    cvar_t *p = (cvar_t *)item;
    int     target_id = *(int *)ctx;
    cvar_t **out_ptr = (cvar_t **)ctx2;

    if (p->cvar_id == target_id && *out_ptr != NULL) {
        *out_ptr = p;
    }
}

int CvarSignal(int cvar_id){

    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return -1;
    }

    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return -1;
    }

    if (queue_size(found_cvar->cvar_waiting_processes) > 0){
        PCB* next = queue_get(found_cvar->cvar_waiting_processes);
        if (next == NULL){
            TracePrintf(0, "CvarInit: No process to signal even though size was greater than 0\n");
            return -1;
        }
        next->state = PCB_READY;
        queue_delete_node(blocked_processes, next);
        queue_add(ready_processes, next);
        TracePrintf(0, "CvarInit: Cvar signaled and process %d awakened\n", next->pid);
        return 0;
    }
    
    return -1; //FIXME: check if this is correct
}

static void broadcast_cvar_cb(void *item, void *ctx, void *ctx2) {
    PCB* p = (PCB *)item;
    p->state = PCB_READY;
    queue_delete_node(blocked_processes, p);
    queue_add(ready_processes, p);
}

int CvarBroadcast(int cvar_id){

    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return -1;
    }

    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return -1;
    }

    if (queue_size(found_cvar->cvar_waiting_processes) > 0){
        queue_iterate(found_cvar->cvar_waiting_processes, broadcast_cvar_cb, NULL, NULL);
        return 0;
    }

    return -1; //FIXME: check if this is correct
}
int CvarWait(int cvar_id, int lock_id){
    if (cvar_id < 1 || lock_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar or lock id\n");
        return -1;
    }

    Release(lock_id);

    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return -1;
    }

    queue_add(found_cvar->cvar_waiting_processes, currentPCB);
    currentPCB->state = PCB_BLOCKED;
    queue_add(blocked_processes, currentPCB);

    PCB* prev = currentPCB;
    if (prev != idlePCB && prev->state == PCB_READY) {
        queue_add(ready_processes, prev);
    }
    PCB* next  = queue_get(ready_processes);
    if (next == NULL) {
        next = idlePCB;
    }
    KernelContextSwitch(KCSwitch, prev, next);
    TracePrintf(0, "CvarInit: Cvar waited and process %d awakened\n", next->pid);
    Acquire(lock_id);
    return 0;
}

int Reclaim_cvar(int cvar_id){
    if (cvar_id < 1){
        TracePrintf(0, "CvarInit: Invalid cvar id\n");
        return -1;
    }

    cvar_t* found_cvar = NULL;
    queue_iterate(cvar_queue, find_cvar_cb, &cvar_id, &found_cvar);
    if (found_cvar == NULL){
        TracePrintf(0, "CvarInit: Cvar not found\n");
        return -1;
    }

    return 0;
}