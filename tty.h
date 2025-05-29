#ifndef _TTY_H
#define _TTY_H

#include "hardware.h"
#include "yalnix.h"
#include "process.h"
#include "queue.h"

typedef struct tty_struct {
    queue_t    *read_queue;
    queue_t    *write_queue;
    char       *read_buffer;
    char       *write_buffer;
    int         read_buffer_size;
    int         write_buffer_size;
    PCB        *current_writer;
    int         using;
    int         write_buffer_position;
} tty_struct_t;

int  SysTtyRead(int tty_id, void *buffer, int size);
int  SysTtyWrite(int tty_id, const void *buffer, int size);
void start_tty_write(int terminal, PCB *writer, void *buffer, int size);
void TtyInit(void);

#endif /* _TTY_H */
