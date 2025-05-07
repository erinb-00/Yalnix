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
/**
 * @brief getFreeFrame: Gets a free frame and returns its number
 * @return int 8*i+j, where i is the byte index and j is the bit index in the bitmap
 */
int getFreeFrame(void);

/**
 * @brief freeFrame: Frees a frame by clearing the corresponding bit in the bitmap
 * @param frame: The frame number to be freed
 */
void freeFrame(int frame);

/**
 * @brief useFrame: Marks a frame as used by setting the corresponding bit in the bitmap
 * @param frame: The frame number to be marked as used
 */
void useFrame(int frame);

/**
 * @brief KernelStart: Initializes the kernel and sets up the environment for the first process
 * @param cmd_args: Command line arguments for the first process
 * @param pmem_size: Size of physical memory
 * @param uctxt: User context for the first process
 */
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);

/** 
 * @brief SetKernelBrk: Sets the kernel break to the specified address
 * @param addr: The address to set the kernel break to
 */
int SetKernelBrk(void *addr);

/**
 * @brief InitTrapTable: Initializes the trap table with appropriate handlers
 */
void InitTrapTable(void);
void KernelInit(void);
void BootstrapInit(UserContext *uctxt);
void LoadInitProcess(UserContext *uctxt);
void EnableVirtualMemory(void);
void DoIdle(void);




#endif // kernel.h