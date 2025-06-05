#include "kernel.h"

void main(void) {
    DoIdle();
}

void DoIdle(void) {
    while (1) {
      TracePrintf(1, "DoIdle\n");  // Log idle trace
      Pause();                     // Halt CPU until next interrupt
    }
}
