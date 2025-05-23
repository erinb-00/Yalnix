#include "hardware.h"
#include "yalnix.h"
#include "ykernel.h"      // for SYS_FORK, SYS_EXEC, … macros
#include "syscalls.h"     // for Fork(), Exec(), Wait(), GetPid(), Brk(), Delay()
#include "kernel.h"
#include "process.h"
#include <string.h>       // for memcpy
#include "queue.h"      // for queue_t to track child PCBs

#define CLONE_TMP1_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 1)
#define CLONE_TMP2_VPN  ((KERNEL_STACK_BASE >> PAGESHIFT) - 2)

static pte_t* CloneUserPageTable(PCB *parent) {
    pte_t *new_pt = calloc(MAX_PT_LEN, sizeof(pte_t));
    if (!new_pt) return NULL;
    // copy each valid region-1 page
    for (int vpn = 0; vpn < MAX_PT_LEN; vpn++) {
        if (!parent->region1_pt[vpn].valid) continue;
        int frame = get_free_frame();
        if (frame < 0) {
            // roll back
            for (int j = 0; j < vpn; j++) {
                if (new_pt[j].valid) free_frame_number(new_pt[j].pfn);
            }
            free(new_pt);
            return NULL;
        }
        // set up new mapping entry
        new_pt[vpn].valid = 1;
        new_pt[vpn].prot  = parent->region1_pt[vpn].prot;
        new_pt[vpn].pfn   = frame;

        // now copy the page’s bytes via two transient kernel mappings
        // map parent page at CLONE_TMP1_VPN
        kernel_page_table[CLONE_TMP1_VPN].valid = 1;
        kernel_page_table[CLONE_TMP1_VPN].prot  = PROT_READ | PROT_WRITE;
        kernel_page_table[CLONE_TMP1_VPN].pfn   = parent->region1_pt[vpn].pfn;
        // map new page at CLONE_TMP2_VPN
        kernel_page_table[CLONE_TMP2_VPN].valid = 1;
        kernel_page_table[CLONE_TMP2_VPN].prot  = PROT_READ | PROT_WRITE;
        kernel_page_table[CLONE_TMP2_VPN].pfn   = frame;
        WriteRegister(REG_TLB_FLUSH, (CLONE_TMP1_VPN << PAGESHIFT));
        WriteRegister(REG_TLB_FLUSH, (CLONE_TMP2_VPN << PAGESHIFT));

        void *src = (void *)(CLONE_TMP1_VPN << PAGESHIFT);
        void *dst = (void *)(CLONE_TMP2_VPN << PAGESHIFT);
        memcpy(dst, src, PAGESIZE);

        // unmap temporaries
        kernel_page_table[CLONE_TMP1_VPN].valid = 0;
        kernel_page_table[CLONE_TMP2_VPN].valid = 0;
        WriteRegister(REG_TLB_FLUSH, (CLONE_TMP1_VPN << PAGESHIFT));
        WriteRegister(REG_TLB_FLUSH, (CLONE_TMP2_VPN << PAGESHIFT));
    }
    return new_pt;
}

int GetPid(void){

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

//=========================================================================
// CP3: still needs to be implemented
//=========================================================================

int Delay(int clock_ticks){

    if (currentPCB == NULL || clock_ticks < 0){
      return ERROR;
    }

    if (clock_ticks == 0) {
      return 0;
    }

}

// replace your stub in syscalls.c with this:
int Fork(UserContext *uctxt) {
  PCB *parent = currentPCB;

  // Clone the user page table
  pte_t *child_pt = CloneUserPageTable(parent);
  if (child_pt == NULL){
    return ERROR;
  }

  // Create the child PCB
  PCB *child = CreatePCB(child_pt, NULL);
  child->parent = parent;
  // Copy user context
  memcpy(&child->uctxt, &parent->uctxt, sizeof(UserContext));

  // Switch into the child (kernel‐stack cloning done by KCCopy)
  KernelContextSwitch(KCCopy, (void*)parent, (void*)child);

  // on return here: currentPCB == child (in child) or parent (in parent)
  if (currentPCB == child) {
      // child: return 0
      *uctxt = child->uctxt;
      WriteRegister(REG_PTBR1, (unsigned int)child->region1_pt);
      WriteRegister(REG_PTLR1, MAX_PT_LEN);
      return 0;
  } else {
      // parent: return child's pid
      *uctxt = parent->uctxt;
      WriteRegister(REG_PTBR1, (unsigned int)parent->region1_pt);
      WriteRegister(REG_PTLR1, MAX_PT_LEN);
      return child->pid;
  }
}

// Replace the current process’s image with a new program
int Exec(char *filename, char *args[]) {

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
// CP4: still needs to be implemented
//=========================================================================
/*
// Wait for one child to exit (block if none ready)
int Wait(int *status_ptr) {
  
    // Check if the current process has any children
    if (currentPCB->children == NULL) {
      TracePrintf(0, "No children to wait for.\n");
      return -1; // No children to wait for
    }
  
    // Check if the current process is a parent of any child processes
    if (queue_find(waiting_process_queue, currentPCB)) { //needs to be defined
      return -1; // Already waiting for a child process
    }
  
    // Add the current process to the waiting queue
    queue_add(waiting_process_queue, currentPCB); //needs to be defined
  
    // Kernel context switch
    int kc = KernelContextSwitch(KCSwitch, currentPCB, NULL);
    if (kc == -1) {
      TracePrintf(0, "Kernel Switch failed during wait.\n");
      Halt();
    }
  
    return 0;
}

*/