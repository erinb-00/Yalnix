#ifndef _TTY_H
#define _TTY_H

#include "hardware.h"
#include "yalnix.h"
#include "process.h"
#include "queue.h"

typedef struct tty {
    queue_t    *read_queue;
    queue_t    *write_queue;
    char       *read_buffer;
    char       *write_buffer;
    int         read_buffer_size;
    int         write_buffer_size;
    PCB        *current_writer;
    int         using;
    int         write_buffer_position;
} tty_t;

extern tty_t tty_struct[NUM_TERMINALS];
void TtyInit(void);
int  user_TtyRead(int tty_id, void *buf, int len);
int  user_TtyWrite(int tty_id, void *buf, int len);

#endif /* _TTY_H */
