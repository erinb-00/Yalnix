#ifndef SYSCALLS_H
#define SYSCALLS_H

/**
 * GetPid
 * Returns the process ID of the calling process.
 */
int GetPid(void);

/**
 * Brk
 * Sets the end of the data segment (program break) to the address 'addr'.
 * Returns 0 on success, or -1 on error.
 */
int Brk(void *addr);

/**
 * Delay
 * Suspends the calling process for the given number of clock ticks.
 * Returns the number of ticks remaining if interrupted, or 0 if fully elapsed.
 */
int Delay(int clock_ticks);

#endif /* SYSCALLS_H */

