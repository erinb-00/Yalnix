#include "hardware.h"
#include "yalnix.h"
#include "yuser.h"
#include "ykernel.h"

#include "syscall.h"
#include "kernel.h"
#include "process.h"
#include "sync.h"

int SysFork(UserContext *uctxt){

}


int SysExec(char *filename, char *argv[]){

}

int SysExit(int status){

}


int SysWait(int *status_ptr){

}

int SysGetPid(void){

}


int SysBrk(void *addr){

}

int SysDelay(int clock_ticks){
    
}
