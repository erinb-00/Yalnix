#ifndef SYNC_CVAR_H
#define SYNC_CVAR_H

#include "queue.h"
#include "process.h"
#include "sync_lock.h"
#include "kernel.h"
#include <limits.h>

// Forward declaration
typedef struct cvar cvar_t;
typedef struct lock lock_t;

// Function declarations
int CvarInit(int *cvar_idp);
int CvarSignal(int cvar_id);
int CvarBroadcast(int cvar_id);
int CvarWait(int cvar_id, int lock_id);
int Reclaim_cvar(int cvar_id);

#endif // SYNC_CVAR_H
