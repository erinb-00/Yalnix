#include "kernel.h"
#include "hardware.h"
#include "yalnix.h"
#include "trap.h"
#include "ykernel.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Flag indicating whether virtual memory is enabled
int vmem_enabled = false;

// Kernel heap break and data/text segment boundaries
unsigned int kernel_brk;
unsigned int kernel_data_start;
unsigned int kernel_data_end;
unsigned int kernel_text_start;
unsigned int kernel_text_end;
unsigned int user_stack_limit;

// Page table pointers for kernel and user modes
pte_t* kernel_page_table;
pte_t* user_page_table;
pte_t** process_page_tables;

// Physical memory management variables
static int num_phys_pages;           // Number of physical frames
static int vm_off_brk;               // Kernel break when VM disabled
static int vm_on_brk;                // Kernel break when VM enabled
static unsigned char *frame_free;    // Bit-vector tracking free frames

/**
 * Idle process: loops forever until interrupted
 */
void DoIdle(void) {
  while (1) {
    TracePrintf(1, "DoIdle\n");  // Log idle trace
    Pause();                     // Halt CPU until next interrupt
  }
}

/**
 * Find and allocate a free physical frame
 * Returns: frame index or -1 if none available
 */
int get_free_frame() {
  const int BITS_PER_BYTE = 8;

  // Scan each byte of the bit-vector
  for (int i = 0; i < num_phys_pages; i++) {
    for (int j = 0; j < BITS_PER_BYTE; j++) {
      // Check if bit j in byte i is 0 (free)
      if ((frame_free[i] & (1 << j)) == 0) {
        // Mark it as used
        frame_free[i] |= (1 << j);
        return i * BITS_PER_BYTE + j;
      }
    }
  }
  return -1;
}

/**
 * Mark a frame as used in the bit-vector
 */
void get_frame_number(int index) {
  frame_free[index / 8] |= (1 << (index % 8));
}

/**
 * Free a previously allocated frame in the bit-vector
 */
void free_frame_number(int index) {
  frame_free[index / 8] &= ~(1 << (index % 8));
}

/**
 * Set up the page tables for kernel and initial user context
 */
static void SetupPageTable(void) {
  // Allocate and zero kernel page table
  kernel_page_table = calloc(MAX_PT_LEN, sizeof(pte_t));
  if (kernel_page_table == NULL) Halt();

  // Invalidate all entries initially
  for (int i = 0; i < MAX_PT_LEN; i++) {
    kernel_page_table[i].valid = 0;
  }

  // Map text pages: read+exec, one-to-one mapping
  for (int p = _first_kernel_text_page; p < _first_kernel_data_page; p++) {
    kernel_page_table[p].valid = 1;
    kernel_page_table[p].prot = PROT_READ | PROT_EXEC;
    get_frame_number(p);            // Reserve this frame
    kernel_page_table[p].pfn = p;   // PFN equals page number
  }

  // Map data pages: read+write
  for (int p = _first_kernel_data_page; p < _orig_kernel_brk_page; p++) {
    kernel_page_table[p].valid = 1;
    kernel_page_table[p].prot = PROT_READ | PROT_WRITE;
    get_frame_number(p);
    kernel_page_table[p].pfn = p;
  }

  // Map kernel stack region
  for (int p = KERNEL_STACK_BASE >> PAGESHIFT;
       p < KERNEL_STACK_LIMIT >> PAGESHIFT;
       p++) {
    kernel_page_table[p].valid = 1;
    kernel_page_table[p].pfn   = p;
    get_frame_number(p);
    kernel_page_table[p].prot  = PROT_READ | PROT_WRITE;
  }

  // Allocate empty user page table
  user_page_table = calloc(MAX_PT_LEN, sizeof(pte_t));
  if (user_page_table == NULL) Halt();
  for (int i = 0; i < MAX_PT_LEN; i++) {
    user_page_table[i].valid = 0;
  }

  // Give the user stack one free frame at top of space
  int index = MAX_PT_LEN - 1;
  int frame = get_free_frame();
  user_page_table[index].valid = 1;
  user_page_table[index].pfn   = frame;
  user_page_table[index].prot  = PROT_READ | PROT_WRITE;

  // Load base/limit registers for kernel (PTBR0) and user (PTBR1)
  WriteRegister(REG_PTBR0, (unsigned int)kernel_page_table);
  WriteRegister(REG_PTLR0, MAX_PT_LEN);
  WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);
}

/**
 * Kernel entry point: initializes memory, traps, and idle process
 */
void KernelStart(char *cmd_args[], unsigned int pmem_size,
                 UserContext *uctxt) {
  // Save original break point
  vm_on_brk = _orig_kernel_brk_page;

  // Compute total physical pages and initialize free map
  num_phys_pages = (int)(pmem_size >> PAGESHIFT) / 8;
  frame_free = calloc(num_phys_pages, sizeof(unsigned char));
  for (int i = 0; i < num_phys_pages; i++) {
    frame_free[i] = 0;
  }

  // Build initial page tables
  SetupPageTable();

  // Enable virtual memory hardware
  WriteRegister(REG_VM_ENABLE, 1);
  vmem_enabled = true;

  // Initialize trap handling and exception vector
  TrapInit();
  WriteRegister(REG_VECTOR_BASE, (unsigned int)interruptVector);

  // Create and initialize idle PCB
  idlePCB = malloc(sizeof(PCB));
  if (!idlePCB) Halt();
  idlePCB->region1_pt = user_page_table;
  idlePCB->pid = helper_new_pid(user_page_table);

  // Set up idle kernel stack frames
  {
    int start = KERNEL_STACK_BASE >> PAGESHIFT;
    int end   = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int n     = end - start;
    for (int i = 0; i < n; i++) {
      idlePCB->kstack_pfn[i] = kernel_page_table[start + i].pfn;
    }
  }

  // Initialize user context for idle process
  idlePCB->uctxt   = *uctxt;
  idlePCB->uctxt.pc = (void*)DoIdle;
  idlePCB->uctxt.sp = (void*)(VMEM_1_LIMIT - 1);

  currentPCB = idlePCB;

  // Return to user context to start idle loop
  *uctxt = idlePCB->uctxt;
  WriteRegister(REG_PTBR1, (unsigned int)idlePCB->region1_pt);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);

  TracePrintf(1, "leaving KernelStart, returning to DoIdle\n");
  return;
}

/**
 * Adjust the kernel heap break (sbrk-like) for kernel allocations
 * Returns 0 on success, -1 on failure
 */
int SetKernelBrk(void *addr) {
  unsigned int new_brk = UP_TO_PAGE(addr);

  // Check bounds: cannot shrink below original or exceed VMEM_0_LIMIT
  if (new_brk < _orig_kernel_brk_page || new_brk > VMEM_0_LIMIT) {
    return -1;
  }

  // If VM is disabled, track offline break only
  if (!vmem_enabled) {
    vm_off_brk = new_brk - _orig_kernel_brk_page;
    return 0;
  }

  // Growing the break: allocate new pages
  if (new_brk > vm_on_brk) {
    for (int i = vm_on_brk; i < new_brk; i++) {
      int index = get_free_frame();
      if (index == -1) {
        return -1;  // Out of memory
      }
      kernel_page_table[i].valid = 1;
      kernel_page_table[i].pfn   = index;
      kernel_page_table[i].prot  = PROT_READ | PROT_WRITE;
    }
  }
  // Shrinking the break: free pages
  else if (new_brk < vm_on_brk) {
    for (int i = new_brk; i < vm_on_brk; i++) {
      free_frame_number(kernel_page_table[i].pfn);
      kernel_page_table[i].valid = 0;
    }
  }

  vm_on_brk = new_brk;
  return 0;
}

