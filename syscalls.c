#include "kernel.h"
#include "process.h"

int GetPid(void){
  pcb_t *currentPCB = GetCurrentProcess();
  return currentPCB->pid;

}

int Brk(void *addr){

  if (addr == NULL || currentPCB == NULL){ 
    return -1;
  }

  unsigned int converted_addr = (unsigned int) addr;

  if (converted_addr > VMEM_1_LIMIT || converted_addr < VMEM_1_BASE){
  
    return -1;
  
  }

  int region1_pages = VMEM_1_BASE >> PAGESHIFT;
  int addr_page = (converted_addr >> PAGESHIFT) - region1_pages;
  int curr_brk;

  if (currentPCB->brk == NULL){

    for (int i = 0; i < region1_pages; i++){
      if (currentPCB->region1_pt[i].valid == 0){
        curr_brk = i;
        break;
      }
    }

  } else {
    curr_brk = ((unsigned int)currentPCB->brk >> PAGESHIFT) - region1_pages;
  }

  if (curr_brk > addr_page){

    for (int i = curr_brk; i < addr_page; i++){

      int index = get_free_frame();

      if (index < 0){
        for (int i = curr_brk; i < addr_page; i++){ 
          if (currentPCB->region1_pt[i].valid) {
            free_frame_number(currentPCB->region1_pt[i].pfn);
            currentPCB->region1_pt[i].valid = 0;
          }
        }
        return -1;
      }

      currentPCB->region1_pt[i].pfn = index;
      currentPCB->region1_pt[i].valid = 1;
      currentPCB->region1_pt[i].prot = PROT_READ | PROT_WRITE;

    }

  } else {
  
    for (int i = curr_brk; i <= addr_page; i--){
      if (currentPCB->region1_pt[i].valid){
        free_frame_number(currentPCB->region1_pt[i].pfn);
        currentPCB->region1_pt[i].valid = 0;
        WriteRegister(REG_TLB_FLUSH, ((i + region1_pages) << PAGESHIFT));
      }
    }
  }

  currentPCB->brk = addr;
  return 0;

}

int Delay(int clock_ticks){

    if (currentPCB == NULL || clock_ticks < 0){
      return ERROR;
    }

    if (clock_ticks == 0) {
      return 0;
    }

}

int Fork(UserContext *uctxt) {
  pcb_t *current_pcb = GetCurrentProcess();
  // Create a child PCB
  pcb_t *new_pcb = CreatePCB();
  new_pcb->parent = current_pcb;

  // Copy user context
  memcpy(&new_pcb->user_context, &current_pcb->user_context, sizeof(UserContext));

  // Copy page table
  // CopyPagetable(current_pcb, new_pcb);

  // Call Kernel Context Switch
  int kc = KernelContextSwitch(KCCopy, new_pcb, NULL);
  if (kc == -1) {
    TracePrintf(0, "Kernel Switch failed during fork.\n");
    Halt();
  }

  if(current_process == GetCurrentProcess()) { // Changed to GetCurrentProcess()
    // This is the child process
    WriteRegister(REG_PTBR0, (unsigned int)new_pcb->page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
    return 0;
  } else {
    // This is the parent process
    WriteRegister(REG_PTBR1, (unsigned int)current_pcb->page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
    return new_pcb->pid;
  }
}

int Exec(char *filename, char *args[]) {
  return LoadProgram(filename, args, currentPCB);
}

void Exit(int status) {
  pcb_t *currentPCB = GetCurrentProcess();

  if(currentPCB->pid == 1){
    //DestroyPCB(currentPCB);
    Halt();
  }
  // Set the process state to ZOMBIE
  currentPCB->state = PCB_ZOMBIE;
  pcb_enqueue(zombie_processes, currentPCB);
  currentPCB->exit_status = status;

  // Check if the parent process is waiting for this child process
  pcb_t *parent = currentPCB->parent;
  if (parent && pcb_in_queue(waiting_parent_processes, parent)) {
    // Remove the parent from the waiting queue
    pcb_queue_remove(waiting_parent_processes, parent);
    // Add the parent to the ready queue
    parent->state = PCB_READY;
    pcb_enqueue(ready_processes, parent);
  }

  // Check if the current process is the last child of its parent
  pcb_t *next  = (ready_processes->head == NULL) ? idle_pcb : pcb_dequeue(ready_processes);
  // Kernel context switch
  int kc = KernelContextSwitch(KCSwitch, currentPCB, next);
  if (kc == -1) {
    TracePrintf(0, "Kernel Switch failed during exit.\n");
    Halt();
  }
}

int Wait(int *status) {
  pcb_t *currentPCB = GetCurrentProcess();

  // Check if the current process has any children
  if (currentPCB->children->head == NULL) {
    TracePrintf(0, "No children to wait for.\n");
    return -1; // No children to wait for
  }

  // Check if the current process is a parent of any child processes
  if (pcb_in_queue(waiting_parent_processes, currentPCB)) {
    return -1; // Already waiting for a child process
  }

  // Add the current process to the waiting queue
  pcb_enqueue(waiting_parent_processes, currentPCB);

  // Kernel context switch
  int kc = KernelContextSwitch(KCSwitch, currentPCB, NULL);
  if (kc == -1) {
    TracePrintf(0, "Kernel Switch failed during wait.\n");
    Halt();
  }

  return 0;
}
