#include "process.h"        // Assuming PCB is defined elsewhere
#include "queue.h"      // Assuming queue_t is defined elsewhere
#include "sync_lock.h"
#include "sync_cvar.h"
#include <limits.h>

typedef struct lock {
    int lock_id;
    int lock_state;
    PCB* lock_owner;
    queue_t* lock_waiting_processes;
} lock_t;

static const int LOCK_MAX  = INT_MAX / 3;
static int next_lock_id = 1;

int LockInit(int *lock_idp){

    if (next_lock_id > LOCK_MAX) {
        TracePrintf(0, "LockInit: Maximum number of locks reached\n");
        return -1;
    }

    if (lock_idp == NULL){
        return -1;
    }

    lock_t* new_lock = malloc(sizeof(lock_t));
    if (new_lock == NULL){
        return -1;
    }

    queue_add(locks_queue, new_lock);
    new_lock->lock_id = next_lock_id; //FIXME: check if this is correct
    new_lock->lock_state = 0;
    new_lock->lock_owner = NULL;
    new_lock->lock_waiting_processes = queue_new();
    *lock_idp = new_lock->lock_id;
    next_lock_id++;

    return 0;
}

static void find_lock_cb(void *item, void *ctx, void *ctx2) {
    lock_t *p = (lock_t *)item;
    int     target_id = *(int *)ctx;
    lock_t **out_ptr = (lock_t **)ctx2;

    if (p->lock_id == target_id && *out_ptr != NULL) {
        *out_ptr = p;
    }
}

int Acquire(int lock_id){

    if (lock_id < 1){
        TracePrintf(0, "LockInit: Invalid lock id\n");
        return -1;
    }

    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);
    if (found_lock == NULL){
        TracePrintf(0, "LockInit: Lock not found\n");
        return -1;
    }

    if (found_lock->lock_state == 1){
        TracePrintf(0, "LockInit: Lock is already acquired\n");
        queue_add(found_lock->lock_waiting_processes, currentPCB);
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
        return 0;
    }

    TracePrintf(0, "LockInit: Lock acquired\n");
    found_lock->lock_state = 1;
    found_lock->lock_owner = currentPCB;
    return 0;

}

int Release(int lock_id){
    if (lock_id < 1){
        TracePrintf(0, "LockInit: Invalid lock id\n");
        return -1;
    }

    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);
    if (found_lock == NULL){
        TracePrintf(0, "LockInit: Lock not found\n");
        return -1;
    }

    if (found_lock->lock_state == 0){
        TracePrintf(0, "LockInit: Lock is not acquired\n");
        return -1;
    }

    if (found_lock->lock_owner != currentPCB){
        TracePrintf(0, "LockInit: Lock not owned by current process\n");
        return -1;
    }

    found_lock->lock_state = 0;
    found_lock->lock_owner = NULL;

    if (queue_size(found_lock->lock_waiting_processes) > 0){
        PCB* next = queue_get(found_lock->lock_waiting_processes);
        if (next == NULL){
            TracePrintf(0, "LockInit: No process to acquire lock even though size was greater than 0\n");
            return -1;
        }
        next->state = PCB_READY;
        queue_delete_node(blocked_processes, next);
        queue_add(ready_processes, next);
        found_lock->lock_state = 1;
        found_lock->lock_owner = next;
        TracePrintf(0, "LockInit: Lock released and process %d acquired lock\n", next->pid);
    }
    
    return 0;
}

int Reclaim_lock(int lock_id){
    if (lock_id < 1){
        TracePrintf(0, "LockInit: Invalid lock id\n");
        return -1;
    }

    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);
    if (found_lock == NULL){
        TracePrintf(0, "LockInit: Lock not found\n");
        return -1;
    }

    return 0;
}