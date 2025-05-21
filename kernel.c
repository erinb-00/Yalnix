#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <load_info.h>
#include "kernel.h"
#include "hardware.h"
#include "yalnix.h"
#include "trap.h"
#include "ykernel.h"
#include "process.h"

int LoadProgram(char *name, char *args[], PCB* proc);

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
PCB* currentPCB = NULL;

/**
 * Idle process: loops forever until interrupted
 */
void DoIdle(void) {
  while (1) {
    TracePrintf(1, "DoIdle\n");  // Log idle trace
    Pause();                     // Halt CPU until next interrupt
  }
}

KernelContext* KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
  PCB *curr = (PCB *)curr_pcb_p;
  PCB *next = (PCB *)next_pcb_p;

  // 1. Save the old kernel context into the current PCB
  curr->kctxt = *kc_in;

  // 2. Remap the kernel stack pages to the next process's frames
  //    Kernel stack lies in Region 0 from KERNEL_STACK_BASE to KERNEL_STACK_LIMIT
  int start = KERNEL_STACK_BASE >> PAGESHIFT;
  int end   = KERNEL_STACK_LIMIT >> PAGESHIFT;
  for (int vpn = start; vpn < end; vpn++) {
    kernel_page_table[vpn].valid = 1;
    kernel_page_table[vpn].prot  = PROT_READ | PROT_WRITE;
    // Map virtual page vpn to the PFN stored in next->kstack_pfn[vpn – start]
    kernel_page_table[vpn].pfn   = next->kstack_pfn[vpn - start];
  }

  // 3. Flush the old stack mappings so the new ones take effect
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);



  // 3b) Switch to the *next* process’s region-1 page table
  WriteRegister(REG_PTBR1, (unsigned int)next->region1_pt);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);
  // 3c) Flush the region-1 TLB so the new mappings take effect
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  currentPCB = next;

  // 4. Return the address of the next PCB's kernel context, so we'll resume there
  return &next->kctxt;
}

KernelContext* KCCopy(KernelContext *kc_in,
                      void *curr_pcb_p,
                      void *next_pcb_p)
{
    PCB   *new_pcb    = (PCB *)next_pcb_p;
    pte_t *kpt        = kernel_page_table;
    const int stack_start = KERNEL_STACK_BASE  >> PAGESHIFT;
    const int stack_end   = KERNEL_STACK_LIMIT >> PAGESHIFT;
    const int n_pages     = stack_end - stack_start;
    const int temp_page   = stack_start - 1;

    /* 1) Copy the incoming kernel context into the new PCB */
    new_pcb->kctxt = *kc_in;

    /* 2) Copy each kernel-stack page into the new PCB's frames */
    for (int i = 0; i < n_pages; i++) {
        /* Map the new stack frame at the temp page */
        kpt[temp_page].valid = 1;
        kpt[temp_page].prot  = PROT_READ | PROT_WRITE;
        kpt[temp_page].pfn   = new_pcb->kstack_pfn[i];
        WriteRegister(REG_TLB_FLUSH, (temp_page << PAGESHIFT));

        /* Copy from current stack page to temp page */
        void *src = (void *)((stack_start + i) << PAGESHIFT);
        void *dst = (void  *)(temp_page           << PAGESHIFT);
        memcpy(dst, src, PAGESIZE);

        /* Unmap the temp page again */
        kpt[temp_page].valid = 0;
        WriteRegister(REG_TLB_FLUSH, (temp_page << PAGESHIFT));
    }

    currentPCB = new_pcb;

    /* Return kc_in so KernelContextSwitch will resume here in the new process */
    return kc_in;
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
  idlePCB = CreatePCB(user_page_table);

  // Set up idle kernel stack frames
  int start = KERNEL_STACK_BASE >> PAGESHIFT;
  int end   = KERNEL_STACK_LIMIT >> PAGESHIFT;
  int n     = end - start;
  for (int i = 0; i < n; i++) {
    //set_kstack_pfn(idlePCB, i, kernel_page_table[start + i].pfn);
    idlePCB->kstack_pfn[i] = kernel_page_table[start + i].pfn;
  }
 
  idlePCB->uctxt = *uctxt;
  idlePCB->uctxt.pc = (void*)DoIdle;
  idlePCB->uctxt.sp = (void*)(VMEM_1_LIMIT - 1);

  currentPCB = idlePCB;

  // Return to user context to start idle loop
  *uctxt = idlePCB->uctxt;
  WriteRegister(REG_PTBR1, (unsigned int)idlePCB->region1_pt);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);

  PCB *initPCB = malloc(sizeof(PCB));
  if (!initPCB){ 
    Halt();
  }

  initPCB->region1_pt = calloc(MAX_PT_LEN, sizeof(pte_t));
  if (!initPCB->region1_pt) Halt();
  for (int i = 0; i < MAX_PT_LEN; i++) {
    initPCB->region1_pt[i].valid = 0;
  }

  initPCB->pid = helper_new_pid(initPCB->region1_pt);

  start = KERNEL_STACK_BASE >> PAGESHIFT;
  end   = KERNEL_STACK_LIMIT >> PAGESHIFT;
  n     = end - start;
  for (int i = 0; i < n; i++) {
    int frame = get_free_frame();
    if (frame < 0) Halt();
    initPCB->kstack_pfn[i] = frame;
  }

  initPCB->uctxt = *uctxt;
  // These will be overwritten by LoadProgram:
  initPCB->uctxt.pc = NULL;
  initPCB->uctxt.sp = (void*)(VMEM_1_LIMIT - 1);

  // Use cmd_args[0] or default to "init"
  char *name = (cmd_args[0] != NULL ? cmd_args[0] : "test/init");
  if (LoadProgram(name, cmd_args, initPCB) != SUCCESS) {
    Halt();
  }

  
  KernelContextSwitch(KCCopy, (void*)idlePCB, (void*)initPCB);

  if (currentPCB == initPCB) {
    return;
  }

  KernelContextSwitch(KCSwitch, (void*)idlePCB, (void*)initPCB);

  //TracePrintf(1, "leaving KernelStart, returning to DoIdle\n");
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

int LoadProgram(char *name, char *args[], PCB* proc) {

  int fd;
  int (*entry)();
  struct load_info li;
  int i;
  char *cp;
  char **cpp;
  char *cp2;
  int argcount;
  int size;
  int text_pg1;
  int data_pg1;
  int data_npg;
  int stack_npg;
  long segment_size;
  char *argbuf;


  /*
   * Open the executable file
   */
  if ((fd = open(name, O_RDONLY)) < 0) {
    TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
    return ERROR;
  }

  if (LoadInfo(fd, &li) != LI_NO_ERROR) {
    TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
    close(fd);
    return (-1);
  }

  if (li.entry < VMEM_1_BASE) {
    TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
    close(fd);
    return ERROR;
  }

  /*
   * Figure out in what region 1 page the different program sections
   * start and end
   */
  text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
  data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
  data_npg = li.id_npg + li.ud_npg;
  /*
   *  Figure out how many bytes are needed to hold the arguments on
   *  the new stack that we are building.  Also count the number of
   *  arguments, to become the argc that the new "main" gets called with.
   */
  size = 0;
  for (i = 0; args[i] != NULL; i++) {
    TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
    size += strlen(args[i]) + 1;
  }
  argcount = i;

  TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

  /*
   *  The arguments will get copied starting at "cp", and the argv
   *  pointers to the arguments (and the argc value) will get built
   *  starting at "cpp".  The value for "cpp" is computed by subtracting
   *  off space for the number of arguments (plus 3, for the argc value,
   *  a NULL pointer terminating the argv pointers, and a NULL pointer
   *  terminating the envp pointers) times the size of each,
   *  and then rounding the value *down* to a double-word boundary.
   */
  cp = ((char *)VMEM_1_LIMIT) - size;

  cpp = (char **)
    (((int)cp -
      ((argcount + 3 + POST_ARGV_NULL_SPACE) *sizeof (void *)))
     & ~7);

  /*
   * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
   * reserved above the stack pointer, before the arguments.
   */
  cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;



  TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n",
              li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);


  /*
   * Compute how many pages we need for the stack */
  stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

  TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n",
              li.t_npg + data_npg, stack_npg);


  /* leave at least one page between heap and stack */
  if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN) {
    close(fd);
    return ERROR;
  }

  /*
   * This completes all the checks before we proceed to actually load
   * the new program.  From this point on, we are committed to either
   * loading succesfully or killing the process.
   */

  /*
   * Set the new stack pointer value in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.sp = cp2;
   */
  proc->uctxt.sp = cp2;

  /*
   * Now save the arguments in a separate buffer in region 0, since
   * we are about to blow away all of region 1.
   */
  cp2 = argbuf = (char *)malloc(size);

  /*
   * ==>> You should perhaps check that malloc returned valid space
   */

  if (argbuf == NULL) {
    TracePrintf(0, "Failed to allocate memory for argbuf\n");
    Halt();
  }

  for (i = 0; args[i] != NULL; i++) {
    TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
    strcpy(cp2, args[i]);
    cp2 += strlen(cp2) + 1;
  }

  /*
   * Set up the page tables for the process so that we can read the
   * program into memory.  Get the right number of physical pages
   * allocated, and set them all to writable.
   */

  /* ==>> Throw away the old region 1 virtual address space by
   * ==>> curent process by walking through the R1 page table and,
   * ==>> for every valid page, free the pfn and mark the page invalid.
   */

  for (int i = 0; i <MAX_PT_LEN; i++){
  
    if (proc->region1_pt[i].valid){
    
      free_frame_number(proc->region1_pt[i].pfn);

      proc->region1_pt[i].valid = 0;
    
    }
  
  }

  /*
   * ==>> Then, build up the new region1.
   * ==>> (See the LoadProgram diagram in the manual.)
   */

  /*
   * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
   * ==>> the "text_pg1" page in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */

  int text_top = text_pg1+ li.t_npg;


  for (int i = text_pg1; i < text_top; i++){
  
    int index = get_free_frame();

    if (index < 0){
    
      for (int i = text_pg1; i < text_top; i++){
      
        if (proc->region1_pt[i].valid) {
        
          free_frame_number(proc->region1_pt[i].pfn);
          proc->region1_pt[i].valid = 0;
          
        }
    
      } 
      return -1;
  
    }
    
    proc->region1_pt[i].pfn = index;
    proc->region1_pt[i].valid = 1;
    proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;

  }




  /*
   * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
   * ==>> the  "data_pg1" in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */

  int heap_top = data_pg1 + data_npg;

  for (int i = data_pg1; i < heap_top; i++){
  
    int index = get_free_frame();

    if (index < 0){
    
      for (int i = data_pg1; i < heap_top;i++){
      
        if (proc->region1_pt[i].valid) {
        
          free_frame_number(proc->region1_pt[i].pfn);
          proc->region1_pt[i].valid = 0;
          
        }
    
      } 
      return -1;
  
    }
    
    proc->region1_pt[i].pfn = index;
    proc->region1_pt[i].valid = 1;
    proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;

  }


  /*
   * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
   * ==>> of the region 1 virtual address space.
   * ==>> These pages should be marked valid, with a
   * ==>> protection of (PROT_READ | PROT_WRITE).
   */

  int stack_start = MAX_PT_LEN -stack_npg;

  for (int i = stack_start; i < MAX_PT_LEN; i++){
  
    int index = get_free_frame();

    if (index < 0){
    
      for (int i = stack_start; i < MAX_PT_LEN;i++){
      
        if (proc->region1_pt[i].valid) {
        
          free_frame_number(proc->region1_pt[i].pfn);
          proc->region1_pt[i].valid = 0;
          
        }
    
      } 
      return -1;
  
    }
    
    proc->region1_pt[i].pfn = index;
    proc->region1_pt[i].valid = 1;
    proc->region1_pt[i].prot = PROT_READ | PROT_WRITE;

  }


  /*
   * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
   */

  // 1) Flush any old region-1 mappings
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  // 2) Tell the MMU to use THIS process page table
  WriteRegister(REG_PTBR1, (unsigned int)proc->region1_pt);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);

  // 3) Flush again to clear any stale entries under the new table
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  /*
   * All pages for the new address space are now in the page table.
   */

  /*
   * Read the text from the file into memory.
   */
  lseek(fd, li.t_faddr, SEEK_SET);
  segment_size = li.t_npg << PAGESHIFT;
  if (read(fd, (void *) li.t_vaddr, segment_size) != segment_size) {
    close(fd);
    return KILL;   // see ykernel.h
  }

  /*
   * Read the data from the file into memory.
   */
  lseek(fd, li.id_faddr, 0);
  segment_size = li.id_npg << PAGESHIFT;

  if (read(fd, (void *) li.id_vaddr, segment_size) != segment_size) {
    close(fd);
    return KILL;
  }


  close(fd);                    /* we've read it all now */


  /*
   * ==>> Above, you mapped the text pages as writable, so this code could write
   * ==>> the new text there.
   *
   * ==>> But now, you need to change the protections so that the machine can execute
   * ==>> the text.
   *
   * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
   * ==>> If any of these page table entries is also in the TLB,
   * ==>> you will need to flush the old mapping.
   */


  for (int i = text_pg1; i < text_top; i++){

    proc->region1_pt[i].prot = PROT_READ | PROT_EXEC;

  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);


  /*
   * Zero out the uninitialized data area
   */
  bzero((void*)li.id_end, li.ud_end - li.id_end);

  /*
   * Set the entry point in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.pc = (caddr_t) li.entry;
   */

  proc->uctxt.pc = (caddr_t)li.entry;

  /*
   * Now, finally, build the argument list on the new stack.
   */


  memset(cpp, 0x00, VMEM_1_LIMIT - ((int) cpp));

  *cpp++ = (char *)argcount;            /* the first value at cpp is argc */
  cp2 = argbuf;
  for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
    *cpp++ = cp;
    strcpy(cp, cp2);
    cp += strlen(cp) + 1;
    cp2 += strlen(cp2) + 1;
  }
  free(argbuf);
  *cpp++ = NULL;                        /* the last argv is a NULL pointer */
  *cpp++ = NULL;                        /* a NULL pointer for an empty envp */

  return SUCCESS;

}
