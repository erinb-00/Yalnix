#include "kernel.h"
#include "process.h"

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

int Delay(int clock_ticks){

    if (currentPCB == NULL || clock_ticks < 0){
      return ERROR;
    }

    if (clock_ticks == 0) {
      return 0;
    }

  }
