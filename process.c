#include "process.h"
#include "ykernel.h"
#include "yalnix.h"
#include "hardware.h"
#include "kernel.h"

PCB* CreatePCB(pte_t* user_page_table, UserContext* uctxt) {

  PCB* newPCB = malloc(sizeof(PCB));

  if (newPCB == NULL){
    Halt();
  }

  //=========================================================================
  // CP2: Keeps track of its user page table
  //=========================================================================
  newPCB->region1_pt = user_page_table;
  newPCB->pid = helper_new_pid(user_page_table);

  //=========================================================================
  // CP2: Keeps track of its User Context
  //=========================================================================
  newPCB->uctxt = *uctxt;

  return newPCB;

}


