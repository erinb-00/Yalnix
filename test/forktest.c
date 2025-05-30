#include "hardware.h"
#include "yalnix.h"
#include "syscalls.h"
#include "yuser.h"
//==================================
// CP3: write a simple init function
//==================================
void main(void) {
    
    int rett = Fork();
    if (rett != 0){
      while (1){
        TracePrintf(1, "got in parent rett:- %d\n", rett);
        Pause();
      }
    } else {
      while(1){
        TracePrintf(1, "got in child rett:- %d\n", rett);
        Pause();
      }

    }

}
