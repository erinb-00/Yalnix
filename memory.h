/* memory.h - Memory management data structures and functions */

#ifndef _MEMORY_H
#define _MEMORY_H

#include "hardware.h"
#include "yalnix.h"

/* Constants */
#define KERNEL_PAGES_COUNT (VMEM_1_SIZE / PAGESIZE)
#define USER_PAGES_COUNT (VMEM_0_SIZE / PAGESIZE)

/* Frame table entry structure */
typedef struct {
    int used;               /* Whether this frame is in use */
    int process_id;         /* PID of the process using this frame, or -1 for kernel */
    unsigned int vaddr;     /* Virtual address mapped to this frame */
} frame_t;

/* Global variables */
extern frame_t *frame_table;        /* Table of all physical frames */
extern unsigned int frame_count;    /* Total number of frames available */

/* Memory management functions */
void MemoryInit(unsigned int pmem_size);
unsigned int AllocatePhysicalPage(void);
void FreePhysicalPage(unsigned int pfn);
void *AllocatePhysicalPages(int count);

/* Page table management */
pte_t *CreateUserPageTable(void);
void DestroyUserPageTable(pte_t *pt);
int MapPage(pte_t *pt, unsigned int vpn, unsigned int pfn, int prot, int is_kernel);
int UnmapPage(pte_t *pt, unsigned int vpn);
int HandlePageFault(void *addr, int access_type);

/* Memory utilities */
void *GetPhysicalAddressFromPFN(unsigned int pfn);
unsigned int VirtualToPhysical(void *vaddr);
void FlushTLB(void);

#endif /* _MEMORY_H */