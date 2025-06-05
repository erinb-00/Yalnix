#ifndef _trap_h
#define _trap_h
#include "hardware.h"
#include "yalnix.h"
#include "sync_lock.h"
#include "sync_cvar.h"
#include "ipc.h"

typedef void (*TrapHandler)(UserContext *uctxt);

extern TrapHandler interruptVector[TRAP_VECTOR_SIZE];

void TrapInit(void);

#endif
