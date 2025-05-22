#include "process.h"
#include "ykernel.h"
#include "yalnix.h"

PCB* CreatePCB(pte_t* user_page_table){

  PCB* newPCB = malloc(sizeof(PCB));

  if (!newPCB){
    Halt();
  }

  newPCB->region1_pt = user_page_table;
  newPCB->pid = helper_new_pid(user_page_table);

  return newPCB;

}

pcb_t *GetCurrentProcess(void){
  return current_process;
}

void SetCurrentProcess(pcb_t *pcb){
  current_process = pcb;
  pcb->state = PCB_RUNNING;
}

