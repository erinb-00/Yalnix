#include <stdio.h>      // for TracePrintf or debugging/logging
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for memset, memcpy
#include "ipc.h"       // for pipe_t, write_node_t, and Pipe functions
#include "queue.h"      // for queue_t and queue functions
#include "process.h"        // for PCB structure and state management
#include "kernel.h"     // for KernelContextSwitch or related kernel functions
#include "hardware.h"  // for PIPE_BUFFER_LEN or other defined constants

//=====================================================================
// Define pipe_t and write_node_t structures
//=====================================================================
typedef struct pipe {
    int pipe_id;
    int read_buffer_position;
    int write_buffer_position;
    queue_t* read_queue;
    queue_t* write_queue;
    unsigned char pipe_data[PIPE_BUFFER_LEN];
    int pipe_data_size;
} pipe_t;

//=====================================================================
// Define write_node_t structure
//=====================================================================
typedef struct write_node {
    void* buf;
    int len;
    PCB* pcb;
} write_node_t;

//=====================================================================
// Define constants
//=====================================================================
static const int CVAR_MAX  = (INT_MAX / 3) * 2; // define CVAR_MAX as 2/3 of INT_MAX
static const int PIPE_MAX  = INT_MAX; // define PIPE_MAX as INT_MAX
static int next_pipe_id = CVAR_MAX + 1; // define next_pipe_id as CVAR_MAX + 1

//=====================================================================
// Define PipeInit function
//=====================================================================
int PipeInit(int* pipe_idp){

    // check if the maximum number of pipes has been reached
    if (next_pipe_id > PIPE_MAX) {
        TracePrintf(0, "PipeInit: Maximum number of pipes reached\n");
        return ERROR;
    }

    // check if the pipe_idp is NULL
    if (pipe_idp == NULL){
        TracePrintf(0, "PipeInit: pipe_idp is NULL\n");
        return ERROR;
    }

    // allocate memory for the pipe
    pipe_t* pipe = (pipe_t*)malloc(sizeof(pipe_t));
    if (pipe == NULL){
        TracePrintf(0, "PipeInit: pipe is NULL\n");
        return ERROR;
    }

    // allocate memory for the read_queue
    pipe->read_queue = queue_new();
    if (pipe->read_queue == NULL){
        TracePrintf(0, "PipeInit: read_queue is NULL\n");
        free(pipe);
        return ERROR;
    }

    // allocate memory for the write_queue
    pipe->write_queue = queue_new();
    if (pipe->write_queue == NULL){
        TracePrintf(0, "PipeInit: write_queue is NULL\n");
        free(pipe->read_queue);
        free(pipe);
        return ERROR;
    }

    // set the pipe_id
    pipe->pipe_id = next_pipe_id; // increment the next_pipe_id
    *pipe_idp = pipe->pipe_id; // set the pipe_idp to the pipe_id
    pipe->read_buffer_position = 0; // set the read_buffer_position to 0
    pipe->write_buffer_position = 0; // set the write_buffer_position to 0
    pipe->pipe_data_size = 0; // set the pipe_data_size to 0
    memset(pipe->pipe_data, 0, sizeof(pipe->pipe_data)); // set the pipe_data to 0
    queue_add(pipes_queue, pipe); // add the pipe to the pipes_queue
    next_pipe_id++; // increment the next_pipe_id
    
    return 0;
}

//=====================================================================
// Define find_pipe_cb function
//=====================================================================
static void find_pipe_cb(void *item, void *ctx, void *ctx2) {
    pipe_t *p = (pipe_t *)item; // cast the item to a pipe_t
    int     target_id = *(int *)ctx; // cast the ctx to an int
    pipe_t **out_ptr = (pipe_t **)ctx2; // cast the ctx2 to a pipe_t **

    // check if the pipe_id of the pipe is equal to the target_id and if the out_ptr is NULL
    if (p->pipe_id == target_id && *out_ptr == NULL) {
        *out_ptr = p;
    }
}

//=====================================================================
// Define PipeRead function
//=====================================================================
int PipeRead(int pipe_id, void* buf, int len){

    // check if the buf is NULL or len is less than or equal to 0 or pipe_id is less than 0
    if (buf == NULL || len <= 0 || pipe_id < 0){
        TracePrintf(0, "PipeRead: buf is NULL or len is less than or equal to 0 or pipe_id is less than 0\n");
        return ERROR;
    }

    // find the pipe with the given pipe_id
    pipe_t* found_pipe = NULL;
    queue_iterate(pipes_queue, find_pipe_cb, &pipe_id, &found_pipe);
    
    // check if the pipe is found
    if (found_pipe == NULL){
        TracePrintf(0, "PipeRead: pipe not found\n");
        return ERROR;
    }

    // check if the pipe is empty
    if (found_pipe->pipe_data_size == 0){
        TracePrintf(0, "PipeRead: pipe found but is empty\n");
        queue_add(found_pipe->read_queue, currentPCB); // add the currentPCB to the read_queue
        currentPCB->state = PCB_BLOCKED; // set the state of the currentPCB to PCB_BLOCKED
        queue_add(blocked_processes, currentPCB); // add the currentPCB to the blocked_processes

        // switch to the idlePCB
        PCB* prev = currentPCB; // set the prev to the currentPCB
        if (prev != idlePCB && prev->state == PCB_READY) { // check if the prev is not the idlePCB and the state of the prev is PCB_READY
            queue_add(ready_processes, prev); // add the prev to the ready_processes
        } 

        // get the next PCB
        PCB* next  = queue_get(ready_processes); // get the next PCB from the ready_processes
        if (next == NULL) { // check if the next is NULL
            next = idlePCB; // set the next to the idlePCB
        }

        // switch to the next PCB
        KernelContextSwitch(KCSwitch, prev, next);
    }

    // check if the pipe is empty
    if(found_pipe->pipe_data_size == 0){
        TracePrintf(0, "PipeRead: pipe found but is empty even after waiting\n");
        return ERROR;
    }

    // set the number of bytes to the pipe_data_size
    int num_bytes = found_pipe->pipe_data_size;
    if(len < found_pipe->pipe_data_size){
        TracePrintf(0, "PipeRead: len is less than pipe_data_size\n");
        num_bytes = len;
    }

    // set the destination to the buf
    char *dst = (char *)buf;
    size_t buf_len = PIPE_BUFFER_LEN;
    size_t ri = found_pipe->read_buffer_position; // set the read_buffer_position to the read_buffer_position

    // set the contiguous to the buf_len minus the read_buffer_position
    size_t contiguous = buf_len - ri;
    if (contiguous > num_bytes) { // check if the contiguous is greater than the num_bytes
        contiguous = num_bytes; // set the contiguous to the num_bytes
    }

    // copy the data from the pipe_data to the buf
    memcpy(dst, found_pipe->pipe_data + ri, contiguous);

    // set the remaining to the num_bytes minus the contiguous
    size_t remaining = num_bytes - contiguous;
    if (remaining > 0) {
        memcpy(dst + contiguous, found_pipe->pipe_data, remaining);
    }

    // set the read_buffer_position to the read_buffer_position plus the num_bytes modulo the buf_len
    found_pipe->read_buffer_position = (ri + num_bytes) % buf_len;

    // set the pipe_data_size to the pipe_data_size minus the num_bytes
    found_pipe->pipe_data_size = found_pipe->pipe_data_size - num_bytes;

    TracePrintf(0, "PipeRead: Read %zu bytes, indexes: read=%zu, write=%d\n", num_bytes, found_pipe->read_buffer_position, found_pipe->write_buffer_position);

    // check if the write_queue is not empty
    while(queue_size(found_pipe->write_queue) > 0){
        // get the write_node from the write_queue
        write_node_t* write_node = queue_get(found_pipe->write_queue);
        if (write_node == NULL){ // check if the write_node is NULL
            TracePrintf(0, "PipeRead: write_node is NULL\n");
            return ERROR;
        }

        // check if the pipe_data_size plus the len of the write_node is greater than the PIPE_BUFFER_LEN
        if (found_pipe->pipe_data_size + write_node->len > PIPE_BUFFER_LEN){
            TracePrintf(0, "PipeRead: pipe_data_size + write_node->len is greater than PIPE_BUFFER_LEN\n");
            break;
        }

        // set the source to the buf of the write_node
        char *src = (char *)write_node->buf;
        int len = write_node->len;
        int space_to_end = PIPE_BUFFER_LEN - found_pipe->write_buffer_position; // set the space_to_end to the PIPE_BUFFER_LEN minus the write_buffer_position
        int first_chunk = (len < space_to_end) ? len : space_to_end;
        memcpy(found_pipe->pipe_data + found_pipe->write_buffer_position, src, first_chunk);
        int second_chunk = len - first_chunk;

        // copy the data from the src to the pipe_data
        if (second_chunk > 0) { // check if the second_chunk is greater than 0
            memcpy(found_pipe->pipe_data, src + first_chunk, second_chunk);
        }

        // set the write_buffer_position to the write_buffer_position plus the len modulo the PIPE_BUFFER_LEN
        found_pipe->write_buffer_position = (found_pipe->write_buffer_position + len) % PIPE_BUFFER_LEN;

        // set the pipe_data_size to the pipe_data_size plus the len
        found_pipe->pipe_data_size += len;

        // set the state of the pcb to PCB_READY
        write_node->pcb->state = PCB_READY;

        // delete the write_node from the blocked_processes
        queue_delete_node(blocked_processes, write_node->pcb);

        // add the write_node to the ready_processes
        queue_add(ready_processes, write_node->pcb);

        // free the write_node
        free(write_node);
        
    }

    return num_bytes;

}

//=====================================================================
// Define PipeWrite function
//=====================================================================
int PipeWrite(int pipe_id, void* buf, int len){

    // check if the buf is NULL or len is less than or equal to 0 or pipe_id is less than or equal to 0
    if (buf == NULL || len <= 0 || pipe_id <= 0){
        TracePrintf(0, "PipeWrite: buf is NULL or len is less than or equal to 0 or pipe_id is less than or equal to 0\n");
        return ERROR;
    }

    // find the pipe with the given pipe_id
    pipe_t* found_pipe = NULL;
    queue_iterate(pipes_queue, find_pipe_cb, &pipe_id, &found_pipe);
    
    // check if the pipe is found
    if (found_pipe == NULL){
        TracePrintf(0, "PipeWrite: pipe not found\n");
        return ERROR;
    }

    int num_bytes = PIPE_BUFFER_LEN - found_pipe->pipe_data_size;
    if(len < num_bytes){
        TracePrintf(0, "PipeWrite: len is less than num_bytes\n");
        num_bytes = len;
    }

    // set the space_to_end to the PIPE_BUFFER_LEN minus the write_buffer_position
    size_t space_to_end = PIPE_BUFFER_LEN - found_pipe->write_buffer_position;

    // set the first_chunk to the num_bytes if the num_bytes is less than or equal to the space_to_end
    size_t first_chunk = (num_bytes <= space_to_end) ? num_bytes : space_to_end;

    // set the source to the buf
    char *src = (char *)buf;

    // copy the data from the src to the pipe_data
    memcpy(&found_pipe->pipe_data[found_pipe->write_buffer_position], src, first_chunk);

    // set the second_chunk to the num_bytes minus the first_chunk
    size_t second_chunk = num_bytes - first_chunk;
    if (second_chunk > 0) { // check if the second_chunk is greater than 0
        memcpy(&found_pipe->pipe_data[0], src + first_chunk, second_chunk);
    }

    // set the write_buffer_position to the write_buffer_position plus the num_bytes modulo the PIPE_BUFFER_LEN
    found_pipe->write_buffer_position = (found_pipe->write_buffer_position + num_bytes) % PIPE_BUFFER_LEN;

    // set the pipe_data_size to the pipe_data_size plus the num_bytes
    found_pipe->pipe_data_size += num_bytes;

    // check if the read_queue is not empty
    if(queue_size(found_pipe->read_queue) > 0){
        // get the pcb from the read_queue
        PCB* pcb = queue_get(found_pipe->read_queue);
        if (pcb == NULL){ // check if the pcb is NULL
            TracePrintf(0, "PipeWrite: pcb is NULL\n");
            return ERROR;
        }
        pcb->state = PCB_READY; // set the state of the pcb to PCB_READY
        queue_delete_node(found_pipe->read_queue, pcb); // delete the pcb from the read_queue
        queue_add(ready_processes, pcb); // add the pcb to the ready_processes
    }

    // check if the num_bytes is equal to the len
    if (num_bytes == len){
        return num_bytes;
    }

    // allocate memory for the write_node
    write_node_t* write_node = (write_node_t*)malloc(sizeof(write_node_t));
    if (write_node == NULL){
        TracePrintf(0, "PipeWrite: write_node is NULL\n");
        return num_bytes;
    }

    // allocate memory for the write_node
    write_node->buf = malloc(len - num_bytes);
    write_node->len = len - num_bytes;
    write_node->pcb = currentPCB; // set the pcb to the currentPCB

    if (write_node->buf == NULL){
        TracePrintf(0, "PipeWrite: write_node->buf is NULL\n");
        free(write_node);
        return num_bytes;
    }

    // copy the data from the buf to the write_node
    memcpy(write_node->buf, (char*)buf + num_bytes, write_node->len);

    // add the write_node to the write_queue
    queue_add(found_pipe->write_queue, write_node);

    // set the state of the currentPCB to PCB_BLOCKED
    currentPCB->state = PCB_BLOCKED;
    queue_add(blocked_processes, currentPCB);

    // get the previous PCB
    PCB* prev = currentPCB;
    if (prev != idlePCB && prev->state == PCB_READY){ // check if the prev is not the idlePCB and the state of the prev is PCB_READY
        queue_add(ready_processes, prev); // add the prev to the ready_processes
    }

    // get the next PCB
    PCB* next = queue_get(ready_processes);
    if (next == NULL){ // check if the next is NULL
        next = idlePCB; // set the next
    }

    KernelContextSwitch(KCSwitch, prev, next);

    return num_bytes;
      
}

//=====================================================================
// Define free_write_node_cb function
//=====================================================================
static void free_write_node_cb(void *item, void *ctx, void *ctx2) {
    write_node_t* write_node = (write_node_t*)item; // cast the item to a write_node_t
    free(write_node);
}

//=====================================================================
// Define Reclaim_pipe function
//=====================================================================
int Reclaim_pipe(int pipe_id){

    // check if the pipe_id is less than 1
    if (pipe_id < 1){
        TracePrintf(0, "PipeInit: Invalid pipe id\n");
        return ERROR;
    }

    // find the pipe with the given pipe_id
    pipe_t* found_pipe = NULL;
    queue_iterate(pipes_queue, find_pipe_cb, &pipe_id, &found_pipe);

    // check if the pipe is found
    if (found_pipe == NULL){
        TracePrintf(0, "PipeInit: Pipe not found\n");
        return ERROR;
    }

    // free the write_queue
    queue_iterate(found_pipe->write_queue, free_write_node_cb, NULL, NULL);

    // delete the pipe from the pipes_queue
    queue_delete_node(pipes_queue, found_pipe);

    // free the read_queue
    free(found_pipe->read_queue);

    // free the write_queue
    free(found_pipe->write_queue);

    // free the pipe
    free(found_pipe);

    // print the pipe reclaimed
    TracePrintf(0, "PipeInit: Pipe reclaimed\n");

    return 0;
}