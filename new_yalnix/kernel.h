#ifndef kernel_h
#define kernel_h

#include <sys/types.h>
#include "hardware.h"
#include "sync.h"
#include "queue.h"
#include "yalnix.h"
#include "ykernel.h"
#include "yuser.h"

// Memory Frame Management
#define NUM_FRAMES(pmem_size) ((pmem_size) / PAGESIZE)
#define BIT_VECTOR_SIZE(num_frames) ((num_frames + 7) / 8) //rounding up

// Kernel Memory Management
#define KERNEL_TEXT_MAX_PAGE (_first_kernel_data_page - 1)
#define KSTACK_PAGES (KERNEL_STACK_MAXSIZE / PAGESIZE)
#define KSTACK_START_PAGE (KERNEL_STACK_BASE >> PAGESHIFT)
#define KERNEL_HEAP_MAX_PAGE (KSTACK_START_PAGE - 1)
#define SCRATCH_ADDR (KERNEL_STACK_BASE - PAGESIZE)

//Virtual Memory Management
#define VMEM_0_PAGES (VMEM_0_SIZE / PAGESIZE)
#define VPN_TO_REGION1_INDEX(vpn) ((vpn) - VMEM_0_PAGES)
#define NUM_PAGES_REGION1 (VMEM_1_SIZE / PAGESIZE)

typedef void (*trap_handler)(UserContext *);

// Kernel Functions

int getFreeFrame(void);

void freeFrame(int frame);

void useFrame(int frame);

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);
int SetKernelBrk(void *addr);
void KernelInit(void);
void BootstrapInit(UserContext *uctxt);
void LoadInitProcess(UserContext *uctxt);
void EnableVirtualMemory(void);

int SetKernelBrk(void *addr);

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);

void DoIdle(void);




#endif // kernel.h