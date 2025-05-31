#include <stdio.h>      // for TracePrintf or debugging/logging
#include <stdlib.h>     // for malloc, free
#include <string.h>     // for memset, memcpy
#include "ipc.h"       // for pipe_t, write_node_t, and Pipe functions
#include "queue.h"      // for queue_t and queue functions
#include "process.h"        // for PCB structure and state management
#include "kernel.h"     // for KernelContextSwitch or related kernel functions
#include "hardware.h"  // for PIPE_BUFFER_LEN or other defined constants

typedef struct pipe {
    int pipe_id;
    int read_buffer_position;
    int write_buffer_position;
    queue_t* read_queue;
    queue_t* write_queue;
    unsigned char pipe_data[PIPE_BUFFER_LEN]; // FIXME: is this correct?
    int pipe_data_size;
} pipe_t;

typedef struct write_node {
    void* buf;
    int len;
    PCB* pcb;
} write_node_t;


int PipeInit(int* pipe_idp){

    if (pipe_idp == NULL){
        TracePrintf(0, "PipeInit: pipe_idp is NULL\n");
        return -1;
    }

    pipe_t* pipe = (pipe_t*)malloc(sizeof(pipe_t));
    if (pipe == NULL){
        TracePrintf(0, "PipeInit: pipe is NULL\n");
        return -1;
    }

    pipe->read_queue = queue_new();
    if (pipe->read_queue == NULL){
        TracePrintf(0, "PipeInit: read_queue is NULL\n");
        free(pipe);
        return -1;
    }

    pipe->write_queue = queue_new();
    if (pipe->write_queue == NULL){
        TracePrintf(0, "PipeInit: write_queue is NULL\n");
        free(pipe->read_queue);
        free(pipe);
        return -1;
    }

    pipe->pipe_id = queue_size(pipes_queue); // FIXME: is this correct? cuz should i instead have a global variable for unique ids?
    *pipe_idp = pipe->pipe_id;
    pipe->read_buffer_position = 0;
    pipe->write_buffer_position = 0;
    pipe->pipe_data_size = 0;
    memset(pipe->pipe_data, 0, sizeof(pipe->pipe_data)); // FIXME: is this correct?
    queue_add(pipes_queue, pipe);

    
    return 0;
}

static void find_pipe_cb(void *item, void *ctx, void *ctx2) {
    pipe_t *p = (pipe_t *)item;
    int     target_id = *(int *)ctx;
    pipe_t **out_ptr = (pipe_t **)ctx2;

    if (p->pipe_id == target_id && *out_ptr != NULL) {
        *out_ptr = p;
    }
}

int PipeRead(int pipe_id, void* buf, int len){

    if (buf == NULL || len <= 0 || pipe_id < 0){
        TracePrintf(0, "PipeRead: buf is NULL or len is less than or equal to 0 or pipe_id is less than 0\n");
        return -1;
    }

    pipe_t* found_pipe = NULL;
    queue_iterate(pipes_queue, find_pipe_cb, &pipe_id, &found_pipe);

    if (found_pipe == NULL){
        TracePrintf(0, "PipeRead: pipe not found\n");
        return -1;
    }
    
    if (found_pipe->pipe_data_size == 0){
        TracePrintf(0, "PipeRead: pipe found but is empty\n");
        queue_add(found_pipe->read_queue, currentPCB);
        currentPCB->state = PCB_BLOCKED;
        queue_add(blocked_processes, currentPCB);

        PCB* prev = currentPCB;
        if (prev != idlePCB && prev->state == PCB_READY) {
            queue_add(ready_processes, prev);
        } 
        PCB* next  = queue_get(ready_processes);
        if (next == NULL) {
            next = idlePCB;
        }
        KernelContextSwitch(KCSwitch, prev, next);
    }

    if(found_pipe->pipe_data_size == 0){
        TracePrintf(0, "PipeRead: pipe found but is empty even after waiting\n");
        return -1;
    }

    int num_bytes = found_pipe->pipe_data_size;
    if(len < found_pipe->pipe_data_size){
        TracePrintf(0, "PipeRead: len is less than pipe_data_size\n");
        num_bytes = len;
    }

    // FIXME: is this correct?======================
    char *dst = (char *)buf;
    size_t buf_len = PIPE_BUFFER_LEN;
    size_t ri = found_pipe->read_buffer_position;

    size_t contiguous = buf_len - ri;
    if (contiguous > num_bytes) {
        contiguous = num_bytes;
    }

    memcpy(dst, found_pipe->pipe_data + ri, contiguous);

    size_t remaining = num_bytes - contiguous;
    if (remaining > 0) {
        memcpy(dst + contiguous, found_pipe->pipe_data, remaining);
    }

    found_pipe->read_buffer_position = (ri + num_bytes) % buf_len;

    found_pipe->pipe_data_size = found_pipe->pipe_data_size - num_bytes;

    TracePrintf(0, "PipeRead: Read %zu bytes, indexes: read=%zu, write=%d\n", num_bytes, found_pipe->read_buffer_position, found_pipe->write_buffer_position);
    // FIXME: is this correct?======================

    while(queue_size(found_pipe->write_queue) > 0){
        write_node_t* write_node = queue_get(found_pipe->write_queue);
        if (write_node == NULL){
            TracePrintf(0, "PipeRead: write_node is NULL\n");
            return -1;
        }

        if (found_pipe->pipe_data_size + write_node->len > PIPE_BUFFER_LEN){
            TracePrintf(0, "PipeRead: pipe_data_size + write_node->len is greater than PIPE_BUFFER_LEN\n");
            break;
        }

        char *src = (char *)write_node->buf;
        int len = write_node->len;
        int space_to_end = PIPE_BUFFER_LEN - found_pipe->write_buffer_position;
        int first_chunk = (len < space_to_end) ? len : space_to_end;
        memcpy(found_pipe->pipe_data + found_pipe->write_buffer_position, src, first_chunk);
        int second_chunk = len - first_chunk;

        if (second_chunk > 0) {
            memcpy(found_pipe->pipe_data, src + first_chunk, second_chunk);
        }

        found_pipe->write_buffer_position = (found_pipe->write_buffer_position + len) % PIPE_BUFFER_LEN;
        found_pipe->pipe_data_size += len;

        write_node->pcb->state = PCB_READY;
        queue_delete_node(blocked_processes, write_node->pcb);
        queue_add(ready_processes, write_node->pcb);
        free(write_node);
        
    }

    return num_bytes;

}

int PipeWrite(int pipe_id, void* buf, int len){

    if (buf == NULL || len <= 0 || pipe_id <= 0){
        TracePrintf(0, "PipeWrite: buf is NULL or len is less than or equal to 0 or pipe_id is less than or equal to 0\n");
        return -1;
    }

    pipe_t* found_pipe = NULL;
    queue_iterate(pipes_queue, find_pipe_cb, &pipe_id, &found_pipe);

    if (found_pipe == NULL){
        TracePrintf(0, "PipeWrite: pipe not found\n");
        return -1;
    }

    int num_bytes = PIPE_BUFFER_LEN - found_pipe->pipe_data_size;
    if(len < num_bytes){
        TracePrintf(0, "PipeWrite: len is less than num_bytes\n");
        num_bytes = len;
    }

    size_t space_to_end = PIPE_BUFFER_LEN - found_pipe->write_buffer_position;
    size_t first_chunk = (num_bytes <= space_to_end) ? num_bytes : space_to_end;
    char *src = (char *)buf;

    memcpy(&found_pipe->pipe_data[found_pipe->write_buffer_position], src, first_chunk);

    size_t second_chunk = num_bytes - first_chunk;
    if (second_chunk > 0) {
        memcpy(&found_pipe->pipe_data[0], src + first_chunk, second_chunk);
    }

    found_pipe->write_buffer_position = (found_pipe->write_buffer_position + num_bytes) % PIPE_BUFFER_LEN;
    found_pipe->pipe_data_size += num_bytes;

    if(queue_size(found_pipe->read_queue) > 0){
        PCB* pcb = queue_get(found_pipe->read_queue);
        pcb->state = PCB_READY;
        queue_delete_node(found_pipe->read_queue, pcb);
        queue_add(ready_processes, pcb);
    }

    if (num_bytes == len){
        return num_bytes;
    }

    write_node_t* write_node = (write_node_t*)malloc(sizeof(write_node_t));
    if (write_node == NULL){
        TracePrintf(0, "PipeWrite: write_node is NULL\n");
        return num_bytes;
    }

    write_node->buf = malloc(len - num_bytes);
    write_node->len = len - num_bytes;
    write_node->pcb = currentPCB;

    if (write_node->buf == NULL){
        TracePrintf(0, "PipeWrite: write_node->buf is NULL\n");
        free(write_node);
        return num_bytes;
    }
    memcpy(write_node->buf, (char*)buf + num_bytes, write_node->len);

    queue_add(found_pipe->write_queue, write_node);
    currentPCB->state = PCB_BLOCKED;
    queue_add(blocked_processes, currentPCB);

    PCB* prev = currentPCB;
    if (prev != idlePCB && prev->state == PCB_READY){
        queue_add(ready_processes, prev);
    }

    PCB* next = queue_get(ready_processes);
    if (next == NULL){
        next = idlePCB;
    }

    KernelContextSwitch(KCSwitch, prev, next);

    return num_bytes;
      
}