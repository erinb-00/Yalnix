#ifndef _trap_h
#define _trap_h
#include "hardware.h"
#include "yalnix.h"

typedef void (*TrapHandler)(UserContext *uctxt);

extern TrapHandler interruptVector[TRAP_VECTOR_SIZE];

void TrapInit(void);
void TrapKernelHandler(UserContext *uctxt);
void TrapClockHandler(UserContext *uctxt);
void TrapMemoryHandler(UserContext *uctxt);
#endif
