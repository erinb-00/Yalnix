#ifndef trap_h
#define trap_h
#include "hardware.h"


/**
 * @brief 
 * TrapKernelHandler: handles trap (system call) requests.
 * Looks at system call code in UserContext, and calls the appropriate system call.
 * Returns values from system call to user mode.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapKernelHandler(UserContext *uctxt);

/**
 * @brief 
 * TrapClockHandler: handles clock interrupts.
 * Updates waiting processes, saves current process context, performs context switch to the next process.
 * If no process is ready to run, go to idle.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapClockHandler(UserContext *uctxt);

/**
 * @brief 
 * TrapIllegalHandler: handles illegal instruction traps.
 * Store status code in user context register 0 and terminates the current process.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapIllegalHandler(UserContext *uctxt);

/**
 * @brief
 * TrapMemoryHandler: handles memory access violations.
 * Print error messaege and seg fault.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapMemoryHandler(UserContext *uctxt);

/**
 * @brief
 * TrapMathHandler: handles math exceptions.
 * Terminate current process with ERROR.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapMathHandler(UserContext *uctxt);

/**
 * @brief
 * TrapTtyReceiveHandler: handles TTY receive interrupts.
 * Gets data from terminal, copy to kernel buffer, wake up waiting process.
 * Store data in buffer for future use.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapTtyReceiveHandler(UserContext *uctxt);

/**
 * @brief
 * TrapTtyTransmitHandler: handles TTY interrupts.
 * Transmit remaining data if write operation is not complete.
 * If write operation is complete, wake up waiting process.
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapTtyTransmitHandler(UserContext *uctxt);

/**
 * @brief
 * TrapDiskHandler: handles disk interrupts.
 * Handle disk operation completion interrupts
 * @param uctxt: pointer to the UserContext structure at the time of the trap.
 */
void TrapDiskHandler(UserContext *uctxt);

/**
 * @brief
 * TrapNotHandled: handles unhandled traps.
 * @param uctxt 
 */
void TrapNotHandled(UserContext *uctxt);

#endif /* !trap_h */

