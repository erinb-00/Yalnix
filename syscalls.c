#include "hardware.h"
#include "yalnix.h"
#include "ykernel.h"      // for SYS_FORK, SYS_EXEC, … macros
#include "syscalls.h"     // for Fork(), Exec(), Wait(), GetPid(), Brk(), Delay()
#include "kernel.h"
#include "process.h"
#include <string.h>       // for memcpy
#include "queue.h"      // for queue_t to track child PCBs
#include "sync_lock.h"
#include "sync_cvar.h"
#include "ipc.h"


//=========================================================================
// CP3: implemeneted GetPid() 
//=========================================================================
int user_GetPid(void){
  return currentPCB->pid;
}

//=========================================================================
// CP3: implemented Brk()
//      Set the current process’s break to the specified address
//=========================================================================
int user_Brk(void *addr){
  TracePrintf(0, "s_Brk called with addr: %p\n", addr);
  if (addr == NULL || currentPCB == NULL){ 
    TracePrintf(0, "s_Brk: Invalid address or current PCB is NULL.\n");
    return ERROR;
  }

  // convert the address to an unsigned int
  unsigned int converted_addr = (unsigned int) addr;

  // check if the converted_addr is out of bounds for region 1
  if (converted_addr > VMEM_1_LIMIT || converted_addr < VMEM_1_BASE){
    TracePrintf(0, "s_Brk: Address %p is out of bounds for region 1.\n", addr);
    return ERROR;
  }

  // get the number of pages in region 1
  int region1_pages = VMEM_1_BASE >> PAGESHIFT;
  int addr_page = (converted_addr >> PAGESHIFT) - region1_pages;
  int curr_brk;

  // check if the currentPCB's break is NULL
  if (currentPCB->brk == NULL){
    TracePrintf(0, "s_Brk: Initializing break for process %d.\n", currentPCB->pid);

    // find the first invalid page in the currentPCB's region1_pt
    for (int i = 0; i < region1_pages; i++){
      if (currentPCB->region1_pt[i].valid == 0){
        curr_brk = i;
        break;
      }
    }
    TracePrintf(0, "s_Brk: Current break for process %d is at page %d.\n", currentPCB->pid, curr_brk);

    // allocate frames for the new pages
    for (int i = curr_brk; i < addr_page; i++){
      int index;
      if ((index = get_free_frame()) < 0){
        TracePrintf(0, "s_Brk: No free frames available for process %d.\n", currentPCB->pid);
        return ERROR; // No free frames available
      }
      currentPCB->region1_pt[i].valid = 1;
      currentPCB->region1_pt[i].prot = PROT_READ | PROT_WRITE;
      currentPCB->region1_pt[i].pfn = index;
    }

    // set the currentPCB's break to the converted_addr
    currentPCB->brk = (void *)converted_addr;
    TracePrintf(0, "Process %d set break to %p at page %d.\n", currentPCB->pid, currentPCB->brk, addr_page);
    return 0;
  } 

  // get the current break page
  curr_brk = ((unsigned int)currentPCB->brk >> PAGESHIFT) - region1_pages;
  if (curr_brk < addr_page){
    TracePrintf(0, "s_Brk: Current break for process %d is lower than the new break at %p.\n", currentPCB->pid, currentPCB->brk);

    // allocate frames for the new pages
    for (int i = curr_brk; i < addr_page; i++){
      int index;
      if ((index = get_free_frame()) < 0){
        TracePrintf(0, "s_Brk: No free frames available for process %d.\n", currentPCB->pid);
        return ERROR; // No free frames available
      }
      currentPCB->region1_pt[i].valid = 1;
      currentPCB->region1_pt[i].prot = PROT_READ | PROT_WRITE;
      currentPCB->region1_pt[i].pfn = index;
    }
  } else if (curr_brk > addr_page){
    TracePrintf(0, "s_Brk: Current break for process %d is higher than the new break at %p.\n", currentPCB->pid, currentPCB->brk);

    // free the frames for the old pages
    for (int i = curr_brk-1; i >= addr_page; i--){
      currentPCB->region1_pt[i].valid = 0;
      free_frame_number(currentPCB->region1_pt[i].pfn);
      WriteRegister(REG_TLB_FLUSH, (i << PAGESHIFT) + VMEM_0_SIZE);
    }
  } else{
    TracePrintf(0, "s_Brk: Current break for process %d is at the same page as the new break at %p.\n", currentPCB->pid, currentPCB->brk);
    return ERROR;
  }

  // set the currentPCB's break to the converted_addr
  currentPCB->brk = (void *)converted_addr;
  TracePrintf(0, "Process %d set break to %p at page %d.\n", currentPCB->pid, currentPCB->brk, addr_page);

  return 0;


}

//=========================================================================
// CP3: Delay()
//      Delay the current process for a specified number of clock ticks
//=========================================================================
int user_Delay(int clock_ticks){

  // check if the currentPCB is NULL or the clock_ticks is less than 0
  if (currentPCB == NULL || clock_ticks < 0){
    return ERROR;
  }
  TracePrintf(0, "Process %d is delaying for %d clock ticks.\n", currentPCB->pid, clock_ticks);

  if (clock_ticks == 0) {
    return 0;
  }

  // set the currentPCB's num_delay to the clock_ticks
  currentPCB->num_delay = clock_ticks;

  // set the currentPCB's state to PCB_BLOCKED
  currentPCB->state = PCB_BLOCKED;

  // add the currentPCB to the blocked_processes
  queue_add(blocked_processes, currentPCB);

  // delete the currentPCB from the ready_processes
  queue_delete_node(ready_processes, currentPCB);

  // 2) Decide which PCB to run next
  PCB *prev = currentPCB;

  // get the next PCB
  PCB* next  = queue_get(ready_processes);

  // check if the next is NULL
  if (next == NULL) {
      // set the next to the idlePCB
      next = idlePCB;
  }

  // check if the prev is not the idlePCB and the state of the prev is PCB_READY
  if (prev != idlePCB && prev->state == PCB_READY) {
      queue_add(ready_processes, prev);
  } 

  // switch the kernel context
  KernelContextSwitch(KCSwitch, prev, next);

  return 0;
}

//=========================================================================
// CP4: implemented Exec()
//      Replace the current process’s image with a new program
//=========================================================================
int user_Exec(char *filename, char *args[]) {

  // check if the filename is NULL or the currentPCB is NULL
  if (!filename || !currentPCB){
    return ERROR;
  }
  // LoadProgram does all the heavy lifting (Checkpoint 3)
  if (LoadProgram(filename, args, currentPCB) < 0)
      return ERROR;
  // On success, it never returns here—but in case of oddity:
  return SUCCESS;
}

//=========================================================================
// CP4: implemented Fork()
//      Create a new process that is a copy of the current process
//=========================================================================
int user_Fork(UserContext *uctxt) {
    // Save current user context to parent PCB
    memcpy(&currentPCB->uctxt, uctxt, sizeof(UserContext));
    PCB *parent = currentPCB;
    
    // Allocate new page table for child
    pte_t *child_pt = calloc(MAX_PT_LEN, sizeof(pte_t));
    if (child_pt == NULL) {
        TracePrintf(0, "s_Fork: Failed to allocate child page table\n");
        return ERROR;
    }

    // Copy each valid region-1 page from parent to child
    for (int vpn = 0; vpn < MAX_PT_LEN; vpn++) {
        if (parent->region1_pt[vpn].valid == 0) {
            continue;
        }

        // Get a free physical frame for the child's page
        int child_pfn = get_free_frame();
        if (child_pfn < 0) {
            TracePrintf(0, "s_Fork: No free frames available\n");
            // Roll back: free all allocated frames
            for (int j = 0; j < vpn; j++) {
                if (child_pt[j].valid) {
                    free_frame_number(child_pt[j].pfn);
                }
            }
            free(child_pt);
            return ERROR;
        }

        // Set up child's page table entry
        child_pt[vpn].pfn = child_pfn;
        child_pt[vpn].valid = 1;
        child_pt[vpn].prot = parent->region1_pt[vpn].prot;

        // Use temporary kernel mapping to copy the page
        int clone_tmp_vpn = ((KERNEL_STACK_BASE >> PAGESHIFT) - 1);
        
        // Map child's physical page into kernel space temporarily
        kernel_page_table[clone_tmp_vpn].valid = 1;
        kernel_page_table[clone_tmp_vpn].prot = PROT_READ | PROT_WRITE;
        kernel_page_table[clone_tmp_vpn].pfn = child_pfn;
        WriteRegister(REG_TLB_FLUSH, clone_tmp_vpn << PAGESHIFT);

        // Copy from parent's virtual address to child's physical page
        void *dst = (void *)(clone_tmp_vpn << PAGESHIFT);
        void *src = (void *)((VMEM_1_BASE + (vpn << PAGESHIFT)));
        memcpy(dst, src, PAGESIZE);
        
        // Clean up temporary mapping
        kernel_page_table[clone_tmp_vpn].valid = 0;
        WriteRegister(REG_TLB_FLUSH, clone_tmp_vpn << PAGESHIFT);
    }

    // Create the child PCB
    PCB *child = CreatePCB(child_pt, &parent->uctxt);
    if (child == NULL) {
        TracePrintf(0, "s_Fork: Failed to create child PCB\n");
        // Roll back: free all allocated frames
        for (int j = 0; j < MAX_PT_LEN; j++) {
            if (child_pt[j].valid) {
                free_frame_number(child_pt[j].pfn);
            }
        }
        free(child_pt);
        return ERROR;
    }

    // Set up parent-child relationship
    child->parent = parent;
    
    // Copy additional process state
    child->brk = parent->brk;  // Important: copy the break pointer
    memcpy(&child->uctxt, &parent->uctxt, sizeof(UserContext));

    int start = KERNEL_STACK_BASE >> PAGESHIFT;
    int end = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int KERNEL_STACK_BASE_TO_LIMIT = end - start;

    for (int i = 0; i < KERNEL_STACK_BASE_TO_LIMIT; i++) {
      int frame = get_free_frame();
      if (frame < 0) Halt();
        child->kstack_pfn[i] = frame;
    } 
    TracePrintf(0, "s_Fork: Created child process %d from parent %d\n", 
                child->pid, parent->pid);

    queue_add(parent->children, child);
    queue_add(ready_processes, child);

    TracePrintf(0, "s_Fork: Created child process %d from parent %d\n", 
                child->pid, parent->pid);

    // Perform kernel context switch to clone the kernel stack
    KernelContextSwitch(KCCopy, (void*)child, NULL);

    
    // After KCCopy returns, we're in either parent or child context
    if (currentPCB == child) {
        // We're in the child process
        TracePrintf(0, "s_Fork: In child process %d\n", currentPCB->pid);
        //exit(0);
        
        // Set up child's address space
        WriteRegister(REG_PTBR1, (unsigned int)child->region1_pt);
        WriteRegister(REG_PTLR1, MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
        
        // Copy user context back to the trap frame
        memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
        return 0;  // Child returns 0
    } else {
        // We're in the parent process
        TracePrintf(0, "s_Fork: In parent process %d\n", currentPCB->pid);
        
        // Ensure parent's address space is set up correctly
        memcpy(uctxt, &currentPCB->uctxt, sizeof(UserContext));
        WriteRegister(REG_PTBR1, (unsigned int)parent->region1_pt);
        WriteRegister(REG_PTLR1, MAX_PT_LEN);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
        
        // Copy user context back to the trap frame
        return child->pid;  // Parent returns child's PID
    }
}

//=========================================================================
// CP4: implemented Wait()
//      Wait for a child process to exit
//=========================================================================
int user_Wait(int *status) {

  // Check if the current process has any children
  if (queue_is_empty(currentPCB->children)) {
    TracePrintf(0, "No children to wait for.\n");
    return ERROR; // No children to wait for
  }

  // Check if the current process is a parent waiting for any child processes
  if (queue_find(waiting_parent_processes, currentPCB) != -1) {
    TracePrintf(0, "Already waiting for a child process");
    return ERROR;
  }

  for (queue_node_t* child = zombie_processes->head; child != NULL; child = child->next) {
    PCB* child_pcb = (PCB*)child->item;
    if (child_pcb->parent == currentPCB) {
      *status = child_pcb->exit_status;
      queue_delete_node(currentPCB->children, child_pcb);
      queue_delete_node(zombie_processes, child);
      DeallocatePCB(child_pcb);
      return child_pcb->pid;
    }
  }
  // Add the current process to the waiting queue
  queue_add(waiting_parent_processes, currentPCB);

  currentPCB->state = PCB_BLOCKED;

  PCB* prev = currentPCB;

  if (prev != idlePCB && prev->state == PCB_READY) {
    queue_add(ready_processes, prev);
  } 

  PCB* next  = queue_get(ready_processes);
  if (next == NULL) {
      // If no ready processes, run the idle process
      next = idlePCB;
  }

  // 3) Do the kernel-mode context switch: save prev’s KernelContext,
  //    remap stack pages & switch to next’s region1 PT, flush TLBs
  KernelContextSwitch(KCSwitch, prev, next);

  for (queue_node_t* child = zombie_processes->head; child != NULL; child = child->next) {
    PCB* child_pcb = (PCB*)child->item;
    if (child_pcb->parent == currentPCB) {
      *status = child_pcb->exit_status;
      queue_delete_node(currentPCB->children, child_pcb);
      queue_delete_node(zombie_processes, child);
      DeallocatePCB(child_pcb);
      return child_pcb->pid;
    }
  }

  return 0;
}

//=========================================================================
// CP4: implemented Exit()
//      Exit the current process
//=========================================================================
void user_Exit(int status) {

  if(currentPCB->pid == 1){
    TracePrintf(0, "s_Exit: init process causes halt per instructions\n");
    Halt();
  }
  // Set the process state to ZOMBIE
  currentPCB->state = PCB_ZOMBIE;
  queue_add(zombie_processes, currentPCB);
  currentPCB->exit_status = status;

  // Check if the parent process is waiting for this child process
  PCB*parent = currentPCB->parent;
  if (parent && queue_find(waiting_parent_processes, parent) != -1) {
    // Remove the parent from the waiting queue
    queue_delete_node(waiting_parent_processes, parent);
    // Add the parent to the ready queue
    parent->state = PCB_READY;
    queue_add(ready_processes, parent);
  }

  // 2) Decide which PCB to run next
  PCB *prev = currentPCB;

  PCB* next  = queue_get(ready_processes);
  if (next == NULL) {
      // If no ready processes, run the idle process
      next = idlePCB;
  }

  // 3) Do the kernel-mode context switch: save prev’s KernelContext,
  //    remap stack pages & switch to next’s region1 PT, flush TLBs
  KernelContextSwitch(KCSwitch, prev, next);
}

int Reclaim(int pid){
  if (pid < 1){
      TracePrintf(0, "LockInit: Invalid pid\n");
      return ERROR;
  }
  if(!Reclaim_lock(pid)){
    if(!Reclaim_cvar(pid)){
      if(!Reclaim_pipe(pid)){
        return ERROR;
      }
    }
  }    
}


