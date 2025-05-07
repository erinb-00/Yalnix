#include "hardware.h"
#include "yalnix.h"
#include "trap.h"
#include "syscall.h"

//Constants

void TrapKernelHandler(UserContext *uctxt){
    int syscall_number = uctxt->code;  // Mask to get syscall number

    switch(syscall_number) {
        case YALNIX_FORK:
            TracePrintf(0, "Yalnix Fork syscall invoked\n");
            //handle_fork(uctxt);
            break;
        case YALNIX_EXEC:
            TracePrintf(0, "Yalnix Exec syscall invoked\n");
            //handle_exec(uctxt);
            break;
        case YALNIX_EXIT:
            TracePrintf(0, "Yalnix Exit syscall invoked\n");
            //handle_exit(uctxt);
            break;
        case YALNIX_WAIT:
            TracePrintf(0, "Yalnix Wait syscall invoked\n");
            //handle_wait(uctxt);
            break;
        case YALNIX_GETPID:
            TracePrintf(0, "Yalnix GetPid syscall invoked\n");
            //handle_getpid(uctxt);
            break;
        case YALNIX_BRK:
            TracePrintf(0, "Yalnix Brk syscall invoked\n");
            //handle_brk(uctxt);
            break;
        case YALNIX_DELAY:
            TracePrintf(0, "Yalnix Delay syscall invoked\n");
            //handle_delay(uctxt);
            break;
        case YALNIX_TTY_READ:
            TracePrintf(0, "Yalnix TTY Read syscall invoked\n");
            //handle_tty_read(uctxt);
            break;
        case YALNIX_TTY_WRITE:
            TracePrintf(0, "Yalnix TTY Write syscall invoked\n");
            //handle_tty_write(uctxt);
            break;
        case YALNIX_LOCK_INIT:
            TracePrintf(0, "Yalnix Lock Init syscall invoked\n");
            //handle_lock_init(uctxt);
            break;
        case YALNIX_LOCK_ACQUIRE:
            TracePrintf(0, "Yalnix Lock Acquire syscall invoked\n");
            //handle_lock_acquire(uctxt);
            break;
        case YALNIX_LOCK_RELEASE:
            TracePrintf(0, "Yalnix Lock Release syscall invoked\n");
            //handle_lock_release(uctxt);
            break;
        case YALNIX_CVAR_INIT:
            TracePrintf(0, "Yalnix Condition Variable Init syscall invoked\n");
            //handle_cvar_init(uctxt);
            break;
        case YALNIX_CVAR_WAIT:
            TracePrintf(0, "Yalnix Condition Variable Wait syscall invoked\n");
            //handle_cvar_wait(uctxt);
            break;
        case YALNIX_CVAR_SIGNAL:
            TracePrintf(0, "Yalnix Condition Variable Signal syscall invoked\n");
            //handle_cvar_signal(uctxt);
            break;
        case YALNIX_CVAR_BROADCAST:
            TracePrintf(0, "Yalnix Condition Variable Broadcast syscall invoked\n");
            //handle_cvar_broadcast(uctxt);
            break;
        case YALNIX_PIPE_INIT:
            TracePrintf(0, "Yalnix Pipe Init syscall invoked\n");
            //handle_pipe_init(uctxt);
            break;
        case YALNIX_PIPE_READ:
            TracePrintf(0, "Yalnix Pipe Read syscall invoked\n");
            //handle_pipe_read(uctxt);
            break;
        case YALNIX_PIPE_WRITE:
            TracePrintf(0, "Yalnix Pipe Write syscall invoked\n");
            //handle_pipe_write(uctxt);
            break;
        default:
            TracePrintf(1, "Error: Unknown syscall number %d\n", syscall_number);
    }
}


void TrapClockHandler(UserContext *uctxt){

}
void TrapIllegalHandler(UserContext *uctxt){

}
void TrapMemoryHandler(UserContext *uctxt){

}
void TrapMathHandler(UserContext *uctxt);
void TrapTtyReceiveHandler(UserContext *uctxt);
void TrapTtyTransmitHandler(UserContext *uctxt);
void TrapDiskHandler(UserContext *uctxt);
void InitInterruptVectorTable(void);