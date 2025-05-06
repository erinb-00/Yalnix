#include "kernel.h"
#include "hardware.h"
#include "yalnix.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

bool vmem_enabled = false;
unsigned int kernel_brk;
unsigned int kernel_data_start;
unsigned int kernel_data_end;
unsigned int kernel_text_start;
unsigned int kernel_text_end;
unsigned int user_stack_limit;

pte_t *kernel_page_table;
pte_t **process_page_tables;

static int num_phys_pages;
static int* frame_free;
static int increased_brk;

int get_free_frame(){

  for (int i = 0; i < num_phys_pages;i++){

    if (frame_free[i] == 0){
      return i;
    }

  }
}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {

  num_phys_pages = pmem_size >> PAGESHIFT;
  frame_free = calloc(sizeof(int), num_phy_pages);

  for (int i = 0; i < num_phys_pages; i++){
    frame_free[i] = 0;
  }

  /* Build page tables */
  SetupPageTables();

}

static void SetupPageTables(void) {
    /* Allocate region 0 page table */
    kernel_page_table = calloc(MAX_PT_LEN, sizeof(pte_t));
    for (int i = 0; i < MAX_PT_LEN; i++) {
        kernel_page_table[i].valid = 0;
    }

    /* Identity-map code + data pages */
    for (int p = _first_kernel_text_page; p < _first_kernel_data_page; p++) {
        kernel_page_table[p].valid = 1;
        kernel_page_table[p].prot = PROT_READ || PROT_EXEC;
        int index = get_free_frame();
        frame_free[i] = 1
        kernel_page_table[p].pfn = index;
    }

    for (int p = _first_kernel_data_page; p < _orig_kernel_brk_page; p++) {
        kernel_page_table[p].valid = 1;
        kernel_page_table[p].prot = PROT_READ || PROT_WRITE;
        int index = get_free_frame();
        frame_free[i] = 1
        kernel_page_table[p].pfn = index;
    }

    //process_page_tables = malloc(sizeof(pte_t*) * MAX_PROCS);
}

int SetKernelBrk(void *addr) {

  unsigned int new_brk = UP_TO_PAGE(addr);

  if (new_brk < kernel_data_start || new_brk > VMEM_0_LIMIT){

    return -1;

  }

  if (!vmem_enabled){
    increased_brk = new_brk - _orig_kernel_brk_page;
  }

  for (int i = _orig_kernel_brk_page; i <= new_brk; i++){

  kernel_page_table[i].valid = 1;
  kernel_page_table[i].prot  = PROT_READ | PROT_WRITE;
  kernel_page_table[i].pfn   = i;


  }
  return 0;
}

void DoIdle(void) {
  while(1) {
    TracePrintf(1,"DoIdle\n");
    Pause();
  }
}
