#ifndef PIPE_H
#define PIPE_H

#include <stddef.h>  // for size_t
#include "queue.h"   // assuming you have a queue implementation
#include "process.h"     // assuming you have a PCB struct defined

// Define a constant for the pipe buffer length
#ifndef PIPE_BUFFER_LEN
#define PIPE_BUFFER_LEN 4096  // Default size if not defined elsewhere
#endif

// Pipe data structure
typedef struct pipe pipe_t;

// Write node structure for buffering writes when the pipe is full
typedef struct write_node write_node_t;

// Function prototypes
int PipeInit(int* pipe_idp);
int PipeRead(int pipe_id, void* buf, int len);
int PipeWrite(int pipe_id, void* buf, int len);
int Reclaim_pipe(int pipe_id);

#endif // PIPE_H
