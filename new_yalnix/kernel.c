#include "hardware.h"
#include "yalnix.h"
#include "kernel.h"
#include "syscall.h"
#include "process.h"
#include "sync.h"
#include "trap.h"
#include "queue.h"

//Memory Management Constants
static int bit_vector_size;
static int current_kernel_brk_page;
static trap_handler trap_table[TRAP_VECTOR_SIZE];
static unsigned char *frame_bitmap;
static pte_t *page_table_region0;
static int is_vm_enabled;
static int switch_flag = 0;

//Functions
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
    return -1; // No free frame found.
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

void InitTrapTable(void) {
    // Initialize the trap table with appropriate handlers
    for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
        trap_table[i] = TrapNotHandled;
    }
    trap_table[TRAP_KERNEL] = TrapKernelHandler;
    trap_table[TRAP_CLOCK] = TrapClockHandler;
    trap_table[TRAP_ILLEGAL] = TrapIllegalHandler;
    trap_table[TRAP_MEMORY] = TrapMemoryHandler;
    trap_table[TRAP_MATH] = TrapMathHandler;
    trap_table[TRAP_TTY_RECEIVE] = TrapTtyReceiveHandler;
    trap_table[TRAP_TTY_TRANSMIT] = TrapTtyTransmitHandler;
    trap_table[TRAP_DISK] = TrapDiskHandler;
}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
    TracePrintf(0, "KernelStart: Starting kernel with pmem_size = %u\n", pmem_size);

    // Initialize kernel brk, frames, frame bitmap, and other variables
    current_kernel_brk_page = _orig_kernel_brk_page;
    bit_vector_size = BIT_VECTOR_SIZE(NUM_FRAMES(pmem_size));
    frame_bitmap = (unsigned char *)calloc(bit_vector_size, sizeof(unsigned char));

    // Check if frame bitmap allocation was successful
    if (frame_bitmap == NULL) {
        TracePrintf(0, "KernelStart: Failed to allocate frame bitmap\n");
        Halt();
    }

    // Initialize Region 0 page table
    page_table_region0 = (pte_t *)calloc(VMEM_0_PAGES, sizeof(pte_t));
    // Error on null
    if (page_table_region0 == NULL) {
        TracePrintf(0, "KernelStart: Failed to allocate page table for region 0\n");
        free(frame_bitmap);
        Halt();
    }

    for (int i = 0; i < VMEM_0_PAGES; i++) {
      if ( i < _orig_kernel_brk_page) {
        page_table_region0[i].valid = 1; // Initialize all entries to valid
        page_table_region0[i].pfn = i; // Set the page frame number
        UseFrame(i); // Mark the frame as used
        if (i < _first_kernel_data_page){ // Kernel text region
          page_table_region0[i].prot = PROT_READ | PROT_EXEC; // Set read/execute permissions
        } else {  // Kernel data(heap) region
          page_table_region0[i].prot = PROT_READ | PROT_WRITE; // Set read/write permissions
        }
      }
    }
    
    // Initialize kernel stack
    for (int i = KSTACK_START_PAGE; i < KSTACK_START_PAGE + KSTACK_PAGES; i++) {
        page_table_region0[i].valid = 1; // Initialize all entries to valid
        page_table_region0[i].pfn = i; // Set the page frame number
        UseFrame(i); // Mark the frame as used
        page_table_region0[i].prot = PROT_READ | PROT_WRITE; // Set read/write permissions
    }

    //Initialize idle PCB
    
    // Set the current kernel break page
    current_kernel_brk_page = KERNEL_HEAP_MAX_PAGE;

    KernelInit();
    BootstrapInit(uctxt);
    LoadInitProcess(uctxt);
    EnableVirtualMemory();
}

int SetKernelBrk(void *addr) {
    unsigned int new_brk_page = (unsigned int)addr >> PAGESHIFT;

    // Checking lower bound
    if (new_brk_page < _orig_kernel_brk_page) {
        TracePrintf(0, "SetKernelBrk: New brk lower than original. %p\n", addr);
        return ERROR;
    }

    if (!is_vm_enabled) {
        // If virtual memory is not enabled, track kernel brk change
        int brk_raise = new_brk_page - _orig_kernel_brk_page;
        TracePrintf(0, "SetKernelBrk: Kernel brk change %d\n", brk_raise);
        return 0;
    } else {
        // If virtual memory is enabled, map pages in region 0
        if (new_brk_page > KERNEL_HEAP_MAX_PAGE) {
            TracePrintf(0, "SetKernelBrk: Collides with kernel stack.\n");
            return ERROR;
        }
        if (new_brk_page <= current_kernel_brk_page){
          TracePrintf(0, "SetKernelBrk: Lowering kernel brk to page %d.\n", addr);
          for (int i = new_brk_page; i < current_kernel_brk_page; i++) {
            freeFrame(page_table_region0[i].pfn); // Free the frame
            page_table_region0[i].valid = 0; // Set to invalid
          }
        } else {
          for(int i = current_kernel_brk_page; i < new_brk_page; i++) {
            int frame = getFreeFrame(); // Get a free frame
            if (frame == -1) {
                TracePrintf(0, "SetKernelBrk: No free frames available\n");
                return ERROR;
            }
            TracePrintf(0, "SetKernelBrk: Raising kernel brk to page %d.\n", frame);
            page_table_region0[i].valid = 1; // Set to valid
            page_table_region0[i].pfn = frame; // Set the page frame number
            page_table_region0[i].prot = PROT_READ | PROT_WRITE; // Set read/write permissions
          }
        }
        
        current_kernel_brk_page = new_brk_page; // Update the current kernel break page
        TracePrintf(0, "SetKernelBrk: Kernel brk set to page %d.\n", current_kernel_brk_page);
        return 0; // Success
    }
  }
