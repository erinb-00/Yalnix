
#ifndef _trap_h
#define _trap_h
#include "hardware.h"
#include "yalnix.h"


void TrapKernelHandler(UserContext *uctxt);
void TrapClockHandler(UserContext *uctxt);
void TrapIllegalHandler(UserContext *uctxt);
void TrapMemoryHandler(UserContext *uctxt);
void TrapMathHandler(UserContext *uctxt);
void TrapTtyReceiveHandler(UserContext *uctxt);
void TrapTtyTransmitHandler(UserContext *uctxt);
void TrapDiskHandler(UserContext *uctxt);

#endif
// trap.h