#include "hardware.h"
#include "yalnix.h"
#include "syscalls.h"
#include "yuser.h"
//==================================
// CP3: write a simple init function
//==================================
void main(void) {

    if (Fork() != 0){
      while (1){
        TracePrintf(1, "got in parent\n");
        Pause();
      }
    } else {
      while(1){
        TracePrintf(1, "got in child\n");
        Pause();
      }

    }

}
