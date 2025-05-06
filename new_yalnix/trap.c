#include "hardware.h"
#include "yalnix.h"
#include "trap.h"
#include "syscall.h"

void TrapKernelHandler(UserContext *uctxt){
    int syscall_number = uctxt->code;  // Mask to get syscall number

    switch(syscall_number) {
        case YALNIX_FORK:
            TracePrintf(0, "Yalnix Fork syscall invoked\n");
            //handle_fork(uctxt);
            break;
        case YALNIX_EXEC:
            //handle_exec(uctxt);
            break;
        case YALNIX_EXIT:
            //handle_exit(uctxt);
            break;
        case YALNIX_WAIT:
            //handle_wait(uctxt);
            break;
        case YALNIX_GETPID:
            //handle_getpid(uctxt);
            break;
        case YALNIX_BRK:
            //handle_brk(uctxt);
            break;
        case YALNIX_DELAY:
            //handle_delay(uctxt);
            break;
        case YALNIX_TTY_READ:
            //handle_tty_read(uctxt);
            break;
        case YALNIX_TTY_WRITE:
            //handle_tty_write(uctxt);
            break;
        case YALNIX_LOCK_INIT:
            //handle_lock_init(uctxt);
            break;
        case YALNIX_LOCK_ACQUIRE:
            //handle_lock_acquire(uctxt);
            break;
        case YALNIX_LOCK_RELEASE:
            //handle_lock_release(uctxt);
            break;
        case YALNIX_CVAR_INIT:
            //handle_cvar_init(uctxt);
            break;
        case YALNIX_CVAR_WAIT:
            //handle_cvar_wait(uctxt);
            break;
        case YALNIX_CVAR_SIGNAL:
            //handle_cvar_signal(uctxt);
            break;
        case YALNIX_CVAR_BROADCAST:
            //handle_cvar_broadcast(uctxt);
            break;
        case YALNIX_PIPE_INIT:
            //handle_pipe_init(uctxt);
            break;
        case YALNIX_PIPE_READ:
            //handle_pipe_read(uctxt);
            break;
        case YALNIX_PIPE_WRITE:
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