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

// Temporary virtual pages for CloneUserPageTable
#define CLONE_TMP1_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 1)
#define CLONE_TMP2_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 2)

int GetPid(void);
int Brk(void *addr);
int Delay(int clock_ticks);
int Fork(UserContext *uctxt);
int Exec(char *filename, char *args[]);
int Wait(int *status_ptr);

#endif // SYSCALLS_H
