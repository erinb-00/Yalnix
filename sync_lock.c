#include "process.h"        // Assuming PCB is defined elsewhere
#include "queue.h"      // Assuming queue_t is defined elsewhere
#include "sync_lock.h"
#include "sync_cvar.h"
#include <limits.h>

//=====================================================================
// Define lock_t structure
//=====================================================================
typedef struct lock {
    int lock_id;
    int lock_state;
    PCB* lock_owner;
    queue_t* lock_waiting_processes;
} lock_t;

//=====================================================================
// Define LOCK_MAX and next_lock_id
//=====================================================================
static const int LOCK_MAX  = INT_MAX / 3;
static int next_lock_id = 1;

//=====================================================================
// Define LockInit function
//=====================================================================
int LockInit(int *lock_idp){

    // check if the next_lock_id is greater than LOCK_MAX
    if (next_lock_id > LOCK_MAX) {
        TracePrintf(0, "LockInit: Maximum number of locks reached\n");
        return ERROR;
    }

    // check if the lock_idp is NULL
    if (lock_idp == NULL){
        return ERROR;
    }

    // allocate memory for the new_lock
    lock_t* new_lock = malloc(sizeof(lock_t));
    if (new_lock == NULL){
        return ERROR;
    }

    // add the new_lock to the locks_queue
    queue_add(locks_queue, new_lock);

    // set the lock_id to the next_lock_id
    new_lock->lock_id = next_lock_id;

    // set the lock_state to 0
    new_lock->lock_state = 0;

    new_lock->lock_owner = NULL;
    new_lock->lock_waiting_processes = queue_new();

    // set the lock_idp to the new_lock_id
    *lock_idp = new_lock->lock_id;

    // increment the next_lock_id
    next_lock_id++;

    return 0;
}

//=====================================================================
// Define find_lock_cb function
//=====================================================================
static void find_lock_cb(void *item, void *ctx, void *ctx2) {
    lock_t *p = (lock_t *)item;
    int     target_id = *(int *)ctx;
    lock_t **out_ptr = (lock_t **)ctx2;
    if (p->lock_id == target_id && *out_ptr == NULL) {
        *out_ptr = p;
    }
}

//=====================================================================
// Define Acquire function
//=====================================================================
int Acquire(int lock_id){

    // check if the lock_id is less than 1
    if (lock_id < 1){
        TracePrintf(0, "LockAcquire: Invalid lock id\n");
        return ERROR;
    }

    // find the lock with the given lock_id
    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);

    // check if the lock is found
    if (found_lock == NULL){
        TracePrintf(0, "LockAcquire: Lock not found\n");
        return ERROR;
    }

    // check if the lock is already acquired
    if (found_lock->lock_state == 1){
        TracePrintf(0, "LockAcquire: Lock is already acquired\n");

        // add the currentPCB to the lock_waiting_processes
        queue_add(found_lock->lock_waiting_processes, currentPCB);

        // set the state of the currentPCB to PCB_BLOCKED
        currentPCB->state = PCB_BLOCKED;
        queue_add(blocked_processes, currentPCB);

        // get the previous PCB
        PCB* prev = currentPCB;

        // check if the prev is not the idlePCB and the state of the prev is PCB_READY
        if (prev != idlePCB && prev->state == PCB_READY) {
            queue_add(ready_processes, prev);
        }

        // get the next PCB
        PCB* next  = queue_get(ready_processes);

        // check if the next is NULL
        if (next == NULL) {
            next = idlePCB;
        }

        // switch the kernel context
        KernelContextSwitch(KCSwitch, prev, next);
        return 0;
    }

    // print the lock acquired
    TracePrintf(0, "LockAcquire: Lock acquired\n");

    // set the lock_state to 1
    found_lock->lock_state = 1;

    // set the lock_owner to the currentPCB
    found_lock->lock_owner = currentPCB;
    return 0;

}

//=====================================================================
// Define Release function
//=====================================================================
int Release(int lock_id){

    // check if the lock_id is less than 1
    if (lock_id < 1){
        TracePrintf(0, "LockRelease: Invalid lock id\n");
        return ERROR;
    }

    // find the lock with the given lock_id
    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);

    // check if the lock is found
    if (found_lock == NULL){
        TracePrintf(0, "LockRelease: Lock not found\n");
        return ERROR;
    }

    // check if the lock is not acquired
    if (found_lock->lock_state == 0){
        TracePrintf(0, "LockRelease: Lock is not acquired\n");
        return ERROR;
    }

    // check if the lock is not owned by the current process
    if (found_lock->lock_owner != currentPCB){
        TracePrintf(0, "LockRelease: Lock not owned by current process\n");
        return ERROR;
    }

    // set the lock_state to 0
    found_lock->lock_state = 0;

    // set the lock_owner to NULL
    found_lock->lock_owner = NULL;

    // check if the lock_waiting_processes is not empty
    if (queue_size(found_lock->lock_waiting_processes) > 0){
        // get the next PCB from the lock_waiting_processes
        PCB* next = queue_get(found_lock->lock_waiting_processes);

        // check if the next is NULL
        if (next == NULL){
            TracePrintf(0, "LockRelease: No process to acquire lock even though size was greater than 0\n");
            return ERROR;
        }

        // set the state of the next to PCB_READY
        next->state = PCB_READY;

        // delete the next from the blocked_processes
        queue_delete_node(blocked_processes, next);

        // add the next to the ready_processes
        queue_add(ready_processes, next);

        // set the lock_state to 1
        found_lock->lock_state = 1;

        // set the lock_owner to the next
        found_lock->lock_owner = next;

        // print the lock released and process acquired lock
        TracePrintf(0, "LockRelease: Lock released and process %d acquired lock\n", next->pid);
    }
    
    return 0;
}

//=====================================================================
// Define Reclaim_lock function
//=====================================================================
int Reclaim_lock(int lock_id){

    // check if the lock_id is less than 1
    if (lock_id < 1){
        TracePrintf(0, "LockReclaim: Invalid lock id\n");
        return ERROR;
    }

    // find the lock with the given lock_id
    lock_t* found_lock = NULL;
    queue_iterate(locks_queue, find_lock_cb, &lock_id, &found_lock);

    // check if the lock is found
    if (found_lock == NULL){
        TracePrintf(0, "LockReclaim: Lock not found\n");
        return ERROR;
    }

    // check if the lock is acquired
    if (found_lock->lock_state == 1){
        TracePrintf(0, "LockReclaim: Lock is acquired\n");
        return ERROR;
    }
    // if (found_lock->lock_owner != currentPCB){ //FIXME: check if tHIS IS NEEDED
    //     TracePrintf(0, "LockInit: Lock is not owned by current process\n");
    // }

    // delete the lock from the locks_queue
    queue_delete_node(locks_queue, found_lock);

    // free the lock_waiting_processes
    free(found_lock->lock_waiting_processes);

    // free the lock
    free(found_lock);
    TracePrintf(0, "LockInit: Lock reclaimed\n");

    return 0;
}