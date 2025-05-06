#include "hardware.h"
#include "yalnix.h"
#include "kernel.h"
#include "syscall.h"
#include "process.h"
#include "sync.h"
#include "trap.h"
#include "queue.h"

//Constants
static int bit_vector_size;
static int current_kernel_brk_page;
static int is_vm_enabled;
static unsigned char *frame_bitmap;
static pte_t *page_table_region0;


void DoIdle(void) {
    while(1) {
    TracePrintf(1,"DoIdle\n");
    Pause();
    }
}

int getFreeFrame(void){
    // Check for each bit vector
    for (int i = 0; i < bit_vector_size; i++)
    {
      // Check each bit in vector
      for (int j = 0; j < 8; j++)
      {
        if ((frame_bitmap[i] & (1 << j)) == 0)  // Check if i-th vector j-th bit is free
        {
          frame_bitmap[i] |= (1 << j);          // Mark the bit as used.
          TracePrintf(0, "Getting free frame %d\n", i * 8 + j);   // Return the corresponding frame number.
          return i * 8 + j;
        }
      }
    }
    return -1;
  }

void freeFrame(int frame) {
    int byte_index = frame / 8;
    int bit_index = frame % 8;
    frame_bitmap[byte_index] &= ~(1 << bit_index); // Clear the bit to mark the frame as free.
    TracePrintf(0, "Freeing frame %d\n", frame);
}

void useFrame(int frame) {
    int byte_index = frame / 8;
    int bit_index = frame % 8;
    frame_bitmap[byte_index] |= (1 << bit_index); // Set the bit to mark the frame as used.
    TracePrintf(0, "Using frame %d\n", frame);
}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
    TracePrintf(0, "KernelStart: Starting kernel with pmem_size = %u\n", pmem_size);

    // Initialize kernel brk, frames, frame bitmap, and other variables
    current_kernel_brk_page = _orig_kernel_brk_page;
    bit_vector_size = BIT_VECTOR_SIZE(NUM_FRAMES(pmem_size));
    frame_bitmap = (unsigned char *)calloc(bit_vector_size, sizeof(unsigned char));
    memset(frame_bitmap, 0, bit_vector_size); // Initialize the bitmap to all free frames

    // Check if frame bitmap allocation was successful
    if (frame_bitmap == NULL) {
        TracePrintf(0, "KernelStart: Failed to allocate frame bitmap\n");
        Halt();
    }

    // Initialize Region 0 page table
    page_table_region0 = (pte_t *)calloc(VMEM_0_PAGES, sizeof(pte_t));

    // Set the current kernel break page
    current_kernel_brk_page = KERNEL_HEAP_MAX_PAGE;

    KernelInit();
    BootstrapInit(uctxt);
    LoadInitProcess(uctxt);
    EnableVirtualMemory();
}
