#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "hardware.h"
#include "yalnix.h"
#include "ykernel.h"      // for SYS_FORK, SYS_EXEC, … macros
#include "syscalls.h"     // for Fork(), Exec(), Wait(), GetPid(), Brk(), Delay()
#include "kernel.h"
#include "process.h"
#include <string.h>       // for memcpy
#include "queue.h"      // for queue_t to track child PCBs
#include "sync_lock.h"
#include "sync_cvar.h"
#include "ipc.h"

// Temporary virtual pages for CloneUserPageTable
#define CLONE_TMP1_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 1)
#define CLONE_TMP2_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 2)

int user_GetPid(void);
int user_Brk(void *addr);
int user_Delay(int clock_ticks);
int user_Fork(UserContext *uctxt);
int user_Exec(char *filename, char *args[]);
int user_Wait(int *status_ptr);
int user_Wait(int *status);
void user_Exit(int status);

#endif // SYSCALLS_H
