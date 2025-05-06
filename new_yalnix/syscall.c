#include "hardware.h"
#include "yalnix.h"
#include "syscall.h"
#include "kernel.h"
#include "process.h"
#include "yuser.h"
#include "ykernel.h"
#include "sync.h"


/**
 * @brief 
 * SysFork: Creates a child process that copies the parent process.
 * Allocates a new PCB for the child process, copies the parent's UserContext,
 * and performs a context switch to the child process.
 * @param uctxt: pointer to the parent process's UserContext.
 * @return child: 0 on success, -1 on failure.
 * parent: PID of the child process on success.
 * Stops the system if context switch fails.
 */
int SysFork(UserContext *uctxt);

/**
 * @brief 
 * SysExec: Executes a new program in the current process.
 * Replaces the current process's memory space with the new program.
 * @param filename: name of the program to execute.
 * @param argv: arguments to pass to the new program.
 * @return SUCCESS on success, ERROR on failure.
 */
int SysExec(char *filename, char *argv[]);

/**
 * @brief 
 * SysExit: Terminates the current process.
 * Cleans up resources and performs a context switch to the next process.
 * @param status: exit status of the process.
 * If the process is PID1(init), halts the system.
 */
int SysExit(int status);

/**
 * @brief 
 * SysWait: Waits for a child process to terminate.
 * Blocks the calling process until the specified child process exits.
 * @param status_ptr: Pointer to store the exit status of the child process.
 * @return PID of the terminated child process on success, ERROR on failure.
 */
int SysWait(int *status_ptr);

/**
 * @brief 
 * SysGetPid: Returns the PID of the calling process.
 * @return PID of the calling process.
 */
int SysGetPid(void);

/**
 * @brief 
 * SysBrk: Changes heap size of the calling process.
 * Increases/decreases heap by allocating/deallocating pages.
 * @param addr: address to new break.
 * @return 0 on success, ERROR on failure.
 */
int SysBrk(void *addr);

/**
 * @brief 
 * SysDelay: Blocks the calling process for a specified time.
 * @param clock_ticks: number of ticks to delay.
 * @return 0 on success, ERROR on failure.
 */
int SysDelay(int clock_ticks);
#endif // syscall.h