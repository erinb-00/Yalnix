/**
 * kernel.c - Main kernel implementation
 * 
 * This file contains the core kernel functions, including initialization
 * and the boot process.
 */

 #include "kernel.h"
 #include "hardware.h"
 #include "yalnix.h"
 #include "trap.h"
 #include "memory.h"
 #include "process.h"
 #include <stdlib.h>
 
 /* Global kernel variables definition */
 int vmem_enabled = 0;
 unsigned int kernel_brk;
 unsigned int kernel_data_start;
 unsigned int kernel_data_end;
 unsigned int kernel_text_start;
 unsigned int kernel_text_end;
 unsigned int user_stack_limit;
 
 pte_t *kernel_page_table;
 pte_t **process_page_tables;
 
 /**
  * KernelStart - Entry point for the kernel
  *
  * This function is called by the boot loader. It initializes the kernel
  * and creates the first user process.
  */
 void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
     /* 
      * This is the entry point for the kernel. It gets called twice:
      * 1. First time before virtual memory is enabled
      * 2. Second time after virtual memory is enabled
      */

     
     if (!vmem_enabled) {
         TracePrintf(1, "KernelStart: First call, initializing kernel\n");
         /*
         /* Initialize kernel data segment boundaries 
         kernel_text_start = VMEM_1_BASE;
         kernel_text_end = (unsigned int)&_etext;
         kernel_data_start = (unsigned int)&_etext;
         kernel_data_end = (unsigned int)&_end;
         kernel_brk = kernel_data_end;
         */
         
         TracePrintf(1, "Kernel text: 0x%x - 0x%x\n", kernel_text_start, kernel_text_end);
         TracePrintf(1, "Kernel data: 0x%x - 0x%x\n", kernel_data_start, kernel_data_end);
         
         /* Initialize memory management */
         MemoryInit(pmem_size);
         
         /* Initialize trap vector */
         TrapVectorInit();
         
         /* Enable virtual memory */
         EnableVirtualMemory();
         
         /* This will cause a return from KernelStart and restart with virtual memory enabled */
         WriteRegister(REG_VM_ENABLE, 1);
         vmem_enabled = 1;
         /* First return from KernelStart */
     } else {
         TracePrintf(1, "KernelStart: Second call, with virtual memory enabled\n");
         
         /* Initialize process management */
         ProcessInit();
         
         /* Initialize system call handlers */
         // SyscallInit();
         
         /* Initialize clock and other devices */
         // DeviceInit();
         
         /* Create the init process */
         LoadInitProcess(uctxt);
         
         /* Second return from KernelStart - will start the init process */
     }
 }
 
  /**
  * EnableVirtualMemory - Set up virtual memory before enabling it
  *
  * This function sets up the initial page tables for the kernel before
  * virtual memory is enabled.
  */
 void EnableVirtualMemory(void) {
     TracePrintf(1, "Setting up virtual memory...\n");
     
     /* Create and initialize the kernel's page table */
     kernel_page_table = (pte_t *)AllocatePhysicalPages(KERNEL_PAGES_COUNT);
     
     /* 
      * Pseudocode for setting up the kernel page table:
      * 1. Identity map the kernel text segment (read-only)
      * 2. Identity map the kernel data segment (read-write)
      * 3. Identity map the kernel stack
      * 4. Set up REG_PTBR0 and REG_PTLR0 for the kernel region
      */
     
     /* Map kernel text segment (read-only) */
     /*
     unsigned int text_page_count = (kernel_text_end - kernel_text_start) / PAGESIZE;
     for (int i = 0; i < text_page_count; i++) {
         unsigned int vpn = (kernel_text_start / PAGESIZE) + i;
         kernel_page_table[vpn].valid = 1;
         // kernel_page_table[vpn].kprot = PROT_READ | PROT_EXEC;
         // kernel_page_table[vpn].uprot = 0;  /* Not accessible to user mode */
         //kernel_page_table[vpn].pfn = vpn;  /* Identity mapping */
     }
     /* Map kernel data segment (read-write) */
     /*
     unsigned int data_page_count = (kernel_data_end - kernel_data_start) / PAGESIZE;
     for (int i = 0; i < data_page_count; i++) {
         unsigned int vpn = (kernel_data_start / PAGESIZE) + i;
         kernel_page_table[vpn].valid = 1;
         kernel_page_table[vpn].kprot = PROT_READ | PROT_WRITE;
         kernel_page_table[vpn].uprot = 0;  /* Not accessible to user mode 
         kernel_page_table[vpn].pfn = vpn;  /* Identity mapping
     }
     */
     /* Map kernel heap */
     /* Additional pages as needed for kernel heap 
     
     /* Set page table registers for kernel region 
     /*
     WriteRegister(REG_PTBR0, (unsigned int)kernel_page_table);
     WriteRegister(REG_PTLR0, KERNEL_PAGES_COUNT);
     
     /* Set up an empty page table for user region (will be populated later) 
     pte_t *empty_user_pt = (pte_t *)AllocatePhysicalPages(USER_PAGES_COUNT);
     WriteRegister(REG_PTBR1, (unsigned int)empty_user_pt);
     WriteRegister(REG_PTLR1, USER_PAGES_COUNT);
 }
 */
 
 /**
  * LoadInitProcess - Create the initial user process
  *
  * This function sets up the context for the init process and
  * loads it into memory.
  */
 void LoadInitProcess(UserContext *uctxt) {
     TracePrintf(1, "Creating init process...\n");
     
     /* 
      * Pseudocode for loading the init process:
      * 1. Allocate PCB for init process
      * 2. Set up user page table for init process
      * 3. Load program into memory
      * 4. Set up user stack
      * 5. Initialize user context (PC, SP, etc.)
      */
     
     /* Create PCB for init process */
     PCB *init_pcb = CreateProcess(1); /* PID 1 */
     
     /* Set up a page table for the user region */
     pte_t *user_pt = CreateUserPageTable();
     
     /* Load init program into memory */
     LoadProgram("init", init_pcb, user_pt);
     
     /* Set up user context */
     /* This will be filled by LoadProgram */
     
     /* Set current process to init */
     SetCurrentProcess(init_pcb);
     
     /* Switch to user mode - this happens on return from KernelStart */
 }
 
 /**
  * SetKernelBrk - Set the kernel's break point
  *
  * This function is used to allocate memory for the kernel.
  */
 int SetKernelBrk(void *addr) {
     unsigned int new_brk = (unsigned int)addr;
     
     if (!vmem_enabled) {
         /* Before virtual memory is enabled, just update the break point */
         kernel_brk = new_brk;
         return 0;
     }
     /*
     /* After virtual memory is enabled, allocate and map new pages if needed 
     if (new_brk > kernel_brk) {
         /* Need to allocate more memory 
         unsigned int old_page_boundary = ROUND_UP(kernel_brk, PAGESIZE);
         unsigned int new_page_boundary = ROUND_UP(new_brk, PAGESIZE);
         
         if (new_page_boundary > old_page_boundary) {
             unsigned int pages_needed = (new_page_boundary - old_page_boundary) / PAGESIZE;
             /* Allocate and map pages 
             /* This is pseudocode - actual implementation would use memory management functions 
             for (unsigned int i = 0; i < pages_needed; i++) {
                 unsigned int vpn = (old_page_boundary / PAGESIZE) + i;
                 unsigned int pfn = AllocatePhysicalPage();
                 /* Map the page in the kernel page table 
                 kernel_page_table[vpn].valid = 1;
                 kernel_page_table[vpn].kprot = PROT_READ | PROT_WRITE;
                 kernel_page_table[vpn].uprot = 0;
                 kernel_page_table[vpn].pfn = pfn;
             }
         }
         
     } else if (new_brk < kernel_brk) {
         /* Can optionally free memory here, but not required 
     }
     
     kernel_brk = new_brk;
     return 0;
 }
 */