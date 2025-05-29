#ifndef _TTY_H
#define _TTY_H

#include "hardware.h"
#include "yalnix.h"
#include "process.h"

typdef struct tty_struct {
    pcb_queue_t *read_queue;
    pcb_queue_t *write_queue;
    char *read_buffer;
    char *write_buffer;
    int read_buffer_size;
    int write_buffer_size;
    pcb_t *current_writer;
    int using;
} tty_struct_t;

void TtyInit(void);
void SysTtyRead(int tty_id, void *buffer, int size);
void start_tty_write(int terminal, pcb_t *writer, void *buffer, int size);
void SysTtyWrite(int tty_id, void *buffer, int size);