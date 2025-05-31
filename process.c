#include "process.h"
#include "ykernel.h"
#include "yalnix.h"
#include "hardware.h"
#include "kernel.h"

//============================================
// CP4:- Tracking queues for round-robin
//============================================
queue_t *ready_processes          = NULL;
queue_t *blocked_processes        = NULL;
queue_t *zombie_processes         = NULL;
queue_t *waiting_parent_processes = NULL;
queue_t *pipes_queue              = NULL;

//==============================================
// CP4:- Initialize queues to track processes
//==============================================
void initQueues(void) {
  ready_processes = queue_new();
  blocked_processes = queue_new();
  zombie_processes = queue_new();
  waiting_parent_processes = queue_new();
  pipes_queue = queue_new();

  if (ready_processes == NULL || blocked_processes == NULL || zombie_processes == NULL || waiting_parent_processes == NULL || pipes_queue == NULL) {
    TracePrintf(0, "Failed to initialize process queues\n");
    Halt();
  }
}

//==========================================================================
// CP2:- Allocate and initialize a new PCB with the given user page table
//==========================================================================
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
  newPCB->brk = NULL; // Initialize the break pointer to NULL
  newPCB->exit_status = 0; // Initialize exit status
  newPCB->num_delay = 0; // Initialize delay counter to 0
  newPCB->parent = NULL; // Initialize parent pointer to NULL
  newPCB->children = queue_new(); // Initialize children queue
  newPCB->state = PCB_READY; // Set initial state to READY

  if (newPCB->children == NULL) {
    TracePrintf(0, "Failed to create children queue for new PCB\n");
    free(newPCB);
    Halt();
  }

  //=========================================================================
  // CP2: Keeps track of its User Context
  //=========================================================================
  memcpy(&newPCB->uctxt, uctxt, sizeof(UserContext));

  return newPCB;

}


