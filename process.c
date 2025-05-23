#include "process.h"
#include "ykernel.h"
#include "yalnix.h"
#include "kernel.h"

pcb_queue_t *ready_processes;          // Queue of processes ready to run
pcb_queue_t *blocked_processes;        // Queue of blocked processes
pcb_queue_t *zombie_processes;        // Queue of zombie processes
pcb_queue_t *waiting_parent_processes; // Queue of processes waiting for their children
pcb_t *idle_pcb = NULL;                       // The idle process

void InitProcessQueues(void) {
  ready_processes = pcb_queue_create();
  blocked_processes = pcb_queue_create();
  zombie_processes = pcb_queue_create();
  waiting_parent_processes = pcb_queue_create();
}

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

void DestroyPCB(pcb_t *pcb) {
  if (pcb->children != NULL) {
    // Free the child processes
    while (pcb->children->head != NULL) {
      pcb_t *child = pcb_dequeue(pcb->children);
      DestroyPCB(child);
    }
    free(pcb->children);
  }
  if(pcb->next != NULL){
    // Free the next process in the queue
    pcb->next->prev = pcb->prev;
  }
  if (pcb->prev != NULL){
    // Free the previous process in the queue
    pcb->prev->next = pcb->next;
  }
  // Free the user page table
  free(pcb->region1_pt);
  // Free the kernel stack pages
  for (int i = 0; i < NUM_PAGES_REGION1; i++) {
    free(pcb->kstack_pfn[i]);
  }
  // Free the PCB itself
  free(pcb);
}

void CopyPageTable(pcb_t *parent, pcb_t *child) {
  // Copy the page table from the parent to the child
  for (int i = 0; i < NUM_PAGES_REGION1; i++) {
    child->region1_pt[i] = parent->region1_pt[i];
  }
  // Copy the kernel stack pages from the parent to the child
  for (int i = 0; i < NUM_PAGES_REGION1; i++) {
    child->kstack_pfn[i] = parent->kstack_pfn[i];
  }
}