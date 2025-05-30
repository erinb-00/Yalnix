#include "hardware.h"
#include "yalnix.h"
#include "yuser.h"

void main(void) {
  Delay(10);
  while (1) {
    TracePrintf(1, "init haha\n");  // Log idle trace
    Pause();                     // Halt CPU until next interrupt
  }
}
