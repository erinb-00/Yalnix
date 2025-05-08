#include "kernel.h"
#include "hardware.h"
#include "yalnix.h"
#include "trap.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int vmem_enabled = false;
unsigned int kernel_brk;
unsigned int kernel_data_start;
unsigned int kernel_data_end;
unsigned int kernel_text_start;
unsigned int kernel_text_end;
unsigned int user_stack_limit;

pte_t* kernel_page_table;
pte_t* user_page_table;
pte_t** process_page_tables;

static int num_phys_pages;
static int vm_off_brk;
static int vm_on_brk;
static unsigned char *frame_free;


int get_free_frame() {

  const int BITS_PER_BYTE = 8;

  for (int i = 0; i < num_phys_pages; i++) {
    for (int j = 0; j < BITS_PER_BYTE; j++) {
      if ((frame_free[i] & (1 << j)) == 0) {

        frame_free[i] |= (1 << j);
        return i * BITS_PER_BYTE + j;

      }
    }
  }

  return -1;

}

void get_frame_number(int index){
  frame_free[(index / 8)] |= (1 << (index % 8));
}

void free_frame_number(int index){
  frame_free[(index / 8)] &= ~(1 << (index % 8));
}


static void SetupPageTable(void) {

  kernel_page_table = calloc(MAX_PT_LEN, sizeof(pte_t));

  if (kernel_page_table == NULL) {
    Halt();
  }

  for (int i = 0; i < MAX_PT_LEN; i++) {
    kernel_page_table[i].valid = 0;
  }

  for (int p = _first_kernel_text_page; p < _first_kernel_data_page; p++) {

    kernel_page_table[p].valid = 1;
    kernel_page_table[p].prot = PROT_READ | PROT_EXEC;
    get_frame_number(p);
    kernel_page_table[p].pfn = p;

  }

  for (int p = _first_kernel_data_page; p < _orig_kernel_brk_page; p++) {

    kernel_page_table[p].valid = 1;
    kernel_page_table[p].prot = PROT_READ | PROT_WRITE;
    get_frame_number(p);
    kernel_page_table[p].pfn = p;

  }

  for (int p = KERNEL_STACK_BASE >> PAGESHIFT; p < KERNEL_STACK_MAXSIZE >> PAGESHIFT; p++) {
    kernel_page_table[p].valid = 1;
    kernel_page_table[p].pfn = p;
    get_frame_number(p);
    kernel_page_table[p].prot = PROT_READ | PROT_WRITE;
  }


  user_page_table = calloc(MAX_PT_LEN, sizeof(pte_t));

  if (user_page_table == NULL) {
    Halt();
  }

  for (int i = 0; i <MAX_PT_LEN; i++){
    user_page_table[i].valid = 0;
  }

  int index = MAX_PT_LEN -1;
  int frame = get_free_frame();
  user_page_table[index].valid = 1;
  user_page_table[index].pfn = frame;
  user_page_table[index].prot = PROT_READ | PROT_WRITE;


  WriteRegister(REG_PTBR0, (unsigned int)kernel_page_table);
  WriteRegister(REG_PTLR0, MAX_PT_LEN);
  WriteRegister(REG_PTBR1, (unsigned int)user_page_table);
  WriteRegister(REG_PTLR1, MAX_PT_LEN);

}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {

  vm_on_brk = _orig_kernel_brk_page;

  num_phys_pages = (int)(pmem_size >> PAGESHIFT)/8;
  frame_free = calloc(num_phys_pages, sizeof(unsigned char));

  for (int i = 0; i < num_phys_pages; i++){
    frame_free[i] = 0;
  }

  SetupPageTable();


  TrapInit();
  WriteRegister(REG_VECTOR_BASE, (unsigned int)interruptVector);




}

int SetKernelBrk(void *addr) {

  unsigned int new_brk = UP_TO_PAGE(addr);

  if (new_brk < _orig_kernel_brk_page || new_brk > VMEM_0_LIMIT){
    return -1;
  }

  if (!vmem_enabled){
    vm_off_brk = new_brk - _orig_kernel_brk_page;
    return 0;
  }

  if (new_brk > vm_on_brk) {
    for (int i = vm_on_brk; i < new_brk; i++){

      int index = get_free_frame();

      if (index == -1) {
        return -1;
      }

      kernel_page_table[i].valid = 1;
      kernel_page_table[i].pfn = index;
      kernel_page_table[i].prot = PROT_READ | PROT_WRITE;

    }
  } else if (new_brk < vm_on_brk){
    for (int i = new_brk; i < vm_on_brk; i ++){

      free_frame_number(kernel_page_table[i].pfn);
      kernel_page_table[i].valid = 0;

    }
  }

  vm_on_brk = new_brk;
  return 0;

}

void DoIdle(void) {
  while(1) {
    TracePrintf(1,"DoIdle\n");
    Pause();
  }
}
