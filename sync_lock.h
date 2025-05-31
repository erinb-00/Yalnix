#ifndef SYS_LOCK_H
#define SYS_LOCK_H

#include "process.h"        // Assuming PCB is defined elsewhere
#include "queue.h"      // Assuming queue_t is defined elsewhere
#include "sync_cvar.h"
#include <limits.h>

typedef struct lock lock_t;
typedef struct cvar cvar_t;

// Lock functions
int LockInit(int *lock_idp);
int Acquire(int lock_id);
int Release(int lock_id);
int Reclaim_lock(int lock_id);

#endif // SYS_LOCK_H
