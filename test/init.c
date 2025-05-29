#include "hardware.h"
#include "yalnix.h"

void main(void) {
  while (1) {
    TracePrintf(1, "init haha\n");  // Log idle trace
    Pause();                     // Halt CPU until next interrupt
  }
}
