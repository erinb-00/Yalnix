#include "hardware.h"
#include "yalnix.h"
#include "syscalls.h"
#include "yuser.h"

void main(void) {
    int pid, status;

    pid = Fork();
    if (pid < 0) {
        TracePrintf(0, "init: Fork failed\n");
    }
    if (pid == 0) {
        // Child
        TracePrintf(1, "init: in child (pid=%d)\n", GetPid());
        Delay(5);
        Exit(5);              // exit with status 5
    } else {
        // Parent (init)
        status = Wait(&status);   // wait for child; get its exit status
        while(1){
        TracePrintf(1,
            "init: child %d exited with status %d\n",
            pid, status);
        Pause();
        }
    }
}

