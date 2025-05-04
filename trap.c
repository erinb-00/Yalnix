/**
 * trap.c
 * 
 * Implementation of interrupt, exception, and trap handler functions for Yalnix
 */

 #include "trap.h"
 #include "hardware.h"
 #include "yalnix.h"
 #include <stdlib.h>  // For exit()
 
 /* Function pointer type for trap handlers */
 typedef void (*TrapHandler)(UserContext *);
 
 /* The interrupt vector table */
 static TrapHandler interruptVector[TRAP_VECTOR_SIZE];
 
 /**
  * Handles kernel call trap (system call) from user processes
  */
 void TrapKernelHandler(UserContext *uctxt) {
     TracePrintf(1, "TrapKernelHandler: syscall number %d\n", uctxt->code);
     
     // Determine the type of kernel call based on the registers in uctxt
     // and perform the appropriate action
     int syscall_number = uctxt->code;  // Mask to get syscall number
     
     // Handle different syscall types based on syscall_number
     switch (syscall_number) {
        // Implement system call handling here
        case YALNIX_FORK:
            //handle_fork(uctxt);
            break;
        case YALNIX_EXEC:
            //handle_exec(uctxt);
            break;
        case YALNIX_EXIT:
            //handle_ioctl(uctxt);
            break;
        case YALNIX_WAIT:
            //handle_wait(uctxt);
            break;
        case YALNIX_GETPID:
            //handle_getpid(uctxt);
            break;
        case YALNIX_BRK:
            //handle_brk(uctxt);
            break;
        case YALNIX_DELAY:
            //handle_delay(uctxt);
            break;
        case YALNIX_TTY_READ:
            //handle_tty_read(uctxt);
            break;
        case YALNIX_TTY_WRITE:
            //handle_tty_write(uctxt);
            break;
         
        default:
            TracePrintf(1, "Error: Unknown syscall number %d\n", syscall_number);
            // Set error return value in uctxt
            uctxt->regs[0] = -1;  // Return value in register 0
            break;
     }
 }
 

 /**
  * Handles hardware clock interrupts
  */
 void TrapClockHandler(UserContext *uctxt) {
     TracePrintf(3, "TrapClockHandler: Clock interrupt received\n");
     
    // Perform clock tick processing
    // clock_ticks++;

    // Wake up any processes whose delay has expired
    /*
    NodePtr *node = delay_block_queue_head->next;
    while (node != delay_block_queue_rear) {
        NodePtr *next_node = node->next;
        PCB *proc = node->element;
        if (--proc->delayTime <= 0) {
            // Remove from delay queue
            node->pre->next = node->next;
            node->next->pre = node->pre;
            // Add to ready queue
            node->next = ready_queue_rear;
            node->pre = ready_queue_rear->pre;
            ready_queue_rear->pre->next = node;
            ready_queue_rear->pre = node;
        }
        node = next_node;
    }
    */
    // Check if the current process has used its time slice
    /*
    if (curPCB->pid != idle_pcb->pid) {
        // Save the current process context
        memcpy(&(curPCB->uctxt), uctxt, sizeof(UserContext));
        // Move the current process to the end of the ready queue
        NodePtr *node = ready_queue_head->next;
        while (node != ready_queue_rear) {
            if (node->element == curPCB) {
                // Remove from current position
                node->pre->next = node->next;
                node->next->pre = node->pre;
                // Add to end
                node->next = ready_queue_rear;
                node->pre = ready_queue_rear->pre;
                ready_queue_rear->pre->next = node;
                ready_queue_rear->pre = node;
                break;
            }
            node = node->next;
        }
        // Select the next process to run
        NodePtr *next_node = ready_queue_head->next;
        if (next_node != ready_queue_rear) {
            curPCB = next_node->element;
            // Remove from ready queue
            ready_queue_head->next = next_node->next;
            next_node->next->pre = ready_queue_head;
            // Restore the next process context
            memcpy(uctxt, &(curPCB->uctxt), sizeof(UserContext));
        } else {
            // No ready process; switch to idle process
            curPCB = idle_pcb;
            memcpy(uctxt, &(curPCB->uctxt), sizeof(UserContext));
        }
    }
}
*/
 
 
 /**
  * Handles illegal instruction exceptions
  */
 void TrapIllegalHandler(UserContext *uctxt) {
     // Handle illegal instruction exception
     TracePrintf(1, "TrapIllegalHandler: Illegal instruction at address %p\n", uctxt->pc);
     
     // This typically involves terminating the offending process
     // Example:
     // int pid = get_current_pid();
     // TracePrintf(1, "Process %d terminated due to illegal instruction\n", pid);
     // terminate_process(pid);
     
     // In the absence of proper process termination, we'll exit
     TracePrintf(0, "KERNEL PANIC: Illegal instruction. Halting system.\n");
     Halt();
 }
 
 /**
  * Handles memory access violations
  */
 void TrapMemoryHandler(UserContext *uctxt) {
     // Extract memory fault information
     void *addr = uctxt->addr;  // Faulting address
     int code = uctxt->code;    // Access violation code
     
     TracePrintf(1, "TrapMemoryHandler: Memory fault at address %p, code %d\n", addr, code);
     
     // Check if this is a valid page fault that can be resolved
     if (code == YALNIX_MAPERR) {
         TracePrintf(2, "Address not mapped - could be a valid page fault\n");
         
         // Check if we should map new memory (e.g., for stack growth)
         // If address is within stack growth region:
         //     Allocate new page frame
         //     Map the page
         //     Return to let process continue
         
         // For example:
         // if (is_valid_stack_growth(addr)) {
         //     if (grow_stack(addr) == 0) {
         //         return;  // Successfully handled, let process continue
         //     }
         // }
         
         TracePrintf(1, "Memory access violation: address %p not mapped\n", addr);
         } 
     else if (code == YALNIX_ACCERR) {
         // Permission violation
         TracePrintf("Memory access violation: invalid permissions for address %p\n", addr);
     }
     
     // If we get here, this is a fatal memory error - terminate process
     // Exa