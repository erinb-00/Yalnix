#include "hardware.h"
#include "yalnix.h"

//==================================
// CP3: write a simple init function
//==================================
void main(void) {
  while (1) {
    TracePrintf(1, "init\n");  // Log init trace
    Pause();                     // Halt CPU until next interrupt
  }
}
