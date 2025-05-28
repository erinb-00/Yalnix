#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#include "yalnix.h"
#include "hardware.h"
/**
 * GetPid
 * Returns the process ID of the calling process.
 */
int SysGetPid(void);

/**
 * Brk
 * Sets the end of the data segment (program break) to the address 'addr'.
 * Returns 0 on success, or -1 on error.
 */
int SysBrk(void *addr);

/**
 * Delay
 * Suspends the calling process for the given number of clock ticks.
 * Returns the number of ticks remaining if interrupted, or 0 if fully elapsed.
 */
int SysDelay(int clock_ticks);
int SysFork(UserContext *uctxt);
int SysExec(char *filename, char *args[]);
void SysExit(int status);

int SysWait(int *status);
#endif /* SYSCALLS_H */

