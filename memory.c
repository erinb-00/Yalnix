/**
 * memory.c - Memory management implementation
 * 
 * This file contains functions for managing physical memory and page tables.
 */

 #include "memory.h"
 #include "kernel.h"
 #include <stdlib.h>
 
 /* Global memory management variables */
 frame_t *frame_table;
 unsigned int frame_count;
 
 /**
  * MemoryInit - Initialize memory management
  *
  * This function sets up the frame table and initializes memory management.
  */
 void MemoryInit(unsigned int pmem_size) {
     TracePrintf(1, "Initializing memory management...\n");
     
     /* Calculate number of frames in physical memory */
     frame_count = pmem_size / PAGESIZE;
     
     /* Allocate space for the frame table */
     /* Note: This is done before virtual memory is enabled, so we use direct allocation */
     //frame_table = (frame_t *)&_end;
     /* Update kernel_brk to account for the frame table */
     unsigned int frame_table_size = frame_count * sizeof(frame_t);
     //kernel_brk = (unsigned int)&_end + frame_table_size;
     
     /* Initialize frame table */
     for (unsigned int i = 0; i < frame_count; i++) {
         /* Mark all frames as unused initially */
         frame_table[i].used = 0;
         frame_table[i].process_id = -1;
         frame_table[i].vaddr = 0;
     }
     
     /* Mark frames used by kernel text and data as used */
     unsigned int kernel_start_frame = VMEM_1_BASE / PAGESIZE;
     unsigned int kernel_end_frame = ROUND_UP(kernel_brk, PAGESIZE) / PAGESIZE;
     
     for (unsigned int i = kernel_start_frame; i < kernel_end_frame; i++) {
         frame_table[i].used = 1;
         frame_table[i].process_id = -1;  /* -1 indicates kernel */
         frame_table[i].vaddr = i * PAGESIZE;  /* Identity mapping */
     }
     
     TracePrintf(1, "Memory initialized: %d frames total, %d frames used by kernel\n", 
               frame_count, kernel_end_frame - kernel_start_frame);
 }
 
 /**
  * AllocatePhysicalPage - Allocate a single physical page
  *
  * Returns the PFN of the allocated page, or 0 if no page is available.
  */
 unsigned int AllocatePhysicalPage(void) {
     /* Search for a free frame */
     for (unsigned int i = 0; i < frame_count; i++) {
         if (!frame_table[i].used) {
             /* Found a free frame */
             frame_table[i].used = 1;
             return i;  /* Return the PFN */
         }
     }
     
     /* No free frames available */
     TracePrintf(0, "ERROR: No free physical pages available\n");
     return 0;  /* 0 is invalid (reserved for kernel use) */
 }
 
 /**
  * FreePhysicalPage - Free a physical page
  *
  * Marks the given physical frame as free.
  */
 void FreePhysicalPage(unsigned int pfn) {
     if (pfn < frame_count) {
         frame_table[pfn].used = 0;
         frame_table[pfn].process_id = -1;
         frame_table[pfn].vaddr = 0;
     }
 }
 
 /**
  * AllocatePhysicalPages - Allocate multiple contiguous physical pages
  *
  * Returns a pointer to the first page, or NULL if the allocation fails.
  */
 void *AllocatePhysicalPages(int count) {
     unsigned int start_pfn = 0;
     int contiguous_count = 0;
     
     /* Find 'count' contiguous free frames */
     for (unsigned int i = 0; i < frame_count; i++) {
         if (!frame_table[i].used) {
             if (contiguous_count == 0) {
                 start_pfn = i;
             }
             contiguous_count++;
             
             if (contiguous_count == count) {
                 /* Found enough contiguous pages */
                 break;
             }
         } else {
             /* Reset counter if we find a used frame */
             contiguous_count = 0;
         }
     }
     
     if (contiguous_count < count) {
         /* Not enough contiguous frames available */
         return NULL;
     }
     
     /* Mark the frames as used */
     for (int i = 0; i < count; i++) {
         unsigned int pfn = start_pfn + i;
         frame_table[pfn].used = 1;
         frame_table[pfn].process_id = -1;  /* Kernel allocation */
     }
     
     /* Return the physical address of the first frame */
     return GetPhysicalAddressFromPFN(start_pfn);
 }
 
 /**
  * CreateUserPageTable - Create a page table for a user process
  *
  * Returns a pointer to the new page table, or NULL on failure.
  */
 pte_t *CreateUserPageTable(void) {
     /* Allocate physical memory for the page table */
     pte_t *pt = (pte_t *)AllocatePhysicalPages(1);  /* One page should be enough */
     
     if (pt == NULL) {
         return NULL;
     }
     
     /* Initialize all entries to invalid */
     for (int i = 0; i < USER_PAGES_COUNT; i++) {
         pt[i].valid = 0;
     }
     
     return pt;
 }
 
 /**
  * DestroyUserPageTable - Free a user process page table
  *
  * This also frees all physical pages mapped in the page table.
  */
 void DestroyUserPageTable(pte_t *pt) {
     if (pt == NULL) {
         return;
     }
     
     /* Free all physical pages mapped in this page table */
     for (int i = 0; i < USER_PAGES_COUNT; i++) {
         if (pt[i].valid) {
             FreePhysicalPage(pt[i].pfn);
         }
     }
     
     /* Free the page table itself */
     /* This assumes the page table is exactly one page - may need adjustment */
     unsigned int pt_pfn = VirtualToPhysical((void *)pt) / PAGESIZE;
     FreePhysicalPage(pt_pfn);
 }
 
 /**
  * MapPage - Map a page into a page table
  *
  * Maps the specified physical frame to the virtual page number in the given page table.
  */
 int MapPage(pte_t *pt, unsigned int vpn, unsigned int pfn, int prot, int is_kernel) {
     if (pt == NULL || pfn >= frame_count) {
         return -1;
     }
     
     /* Set up the page table entry */
     pt[vpn].valid = 1;
     pt[vpn].pfn = pfn;
     
     /*
     if (is_kernel) {
         pt[vpn].kprot = prot;
         pt[vpn].uprot = 0;  //Not accessible to user mode 
     } else {
         pt[vpn].uprot = prot;
         pt[vpn].kprot = PROT_ALL;  //Kernel can always access
     }
     
     //Update frame table
     frame_table[pfn].used = 1;
     frame_table[pfn].vaddr = vpn * PAGESIZE;
     //process_id will be set by the caller
     */
     return 0;
 }
 
 
 /**
  * UnmapPage - Unmap a page from a page table
  *
  * Marks the specified virtual page as invalid in the given page table.
  */
 int UnmapPage(pte_t *pt, unsigned int vpn) {
     if (pt == NULL) {
         return -1;
     }
     
     if (pt[vpn].valid) {
         /* Free the physical page */
         FreePhysicalPage(pt[vpn].pfn);
         
         /* Mark the page table entry as invalid */
         pt[vpn].valid = 0;
     }
     
     return 0;
 }
 
 /**
  * HandlePageFault - Handle a page fault
  *
  * This function is called when a page fault occurs. It determines if the fault
  * can be resolved (e.g., by growing the stack) or if it's a genuine error.
  */
 int HandlePageFault(void *addr, int access_type) {
     unsigned int fault_addr = (unsigned int)addr;
     
     /* Check if this is a user or kernel fault */
     if (fault_addr >= VMEM_1_BASE) {
         /* Kernel region fault - this should not happen */
         TracePrintf(0, "ERROR: Page fault in kernel region: addr=%p\n", addr);
         return -1;
     }
     
     //Get current process - pseudocode */
     PCB *current = GetCurrentProcess();
     /*
     //Check if this is a stack growth request
     if (fault_addr < current->user_stack_base && 
         fault_addr >= current->user_stack_limit) {
         //This is a valid stack growth request 
         
         //Calculate new page to map 
         unsigned int vpn = fault_addr / PAGESIZE;
         
         //Allocate a physical page 
         unsigned int pfn = AllocatePhysicalPage();
         if (pfn == 0) {
             //Could not allocate a page 
             return -1;
         }
         
         // Map the page 
         MapPage(current->page_table, vpn, pfn, PROT_READ | PROT_WRITE, 0);
         
         // Update process stack info 
         current->user_stack_limit = vpn * PAGESIZE;
         
         return 0;  // Successfully handled
         
     } 

     //Genuine page fault - cannot be handled 
     TracePrintf(0, "ERROR: Invalid page fault: addr=%p\n", addr);
     return -1;
     */
 }
 
 /**
  * GetPhysicalAddressFromPFN - Get physical address from PFN
  *
  * Converts a physical frame number to its corresponding physical address.
  */
 void *GetPhysicalAddressFromPFN(unsigned int pfn) {
     return (void *)(pfn * PAGESIZE);
 }
 
 /**
  * VirtualToPhysical - Convert virtual address to physical address
  *
  * Uses the appropriate page table to convert a virtual address to physical.
  */
 unsigned int VirtualToPhysical(void *vaddr) {
     unsigned int addr = (unsigned int)vaddr;
     unsigned int vpn, offset;
     pte_t *pt;
     /*
     //Determine which page table to use
     if (addr >= VMEM_1_BASE) {
         /* Kernel address 
         vpn = (addr - VMEM_1_BASE) / PAGESIZE;
         pt = kernel_page_table;
     } else {
         /* User address - use current process page table 
         PCB *current = GetCurrentProcess();
         vpn = addr / PAGESIZE;
         pt = current->page_table;
     }
     
     if (!pt[vpn].valid) {
         /* Invalid page 
         return 0;
     }
     
     offset = addr % PAGESIZE;
     return (pt[vpn].pfn * PAGESIZE) + offset;
     */
 }
 
 /**
  * FlushTLB - Flush the Translation Lookaside Buffer
  *
  * This is called whenever page tables are modified.
  */
 void FlushTLB(void) {
     WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
 }