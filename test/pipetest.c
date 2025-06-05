#include "yuser.h"    /* Yalnix user‐level syscall wrappers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 128

int
main(void)
{
    int pipe_id;
    int status;
    int pid;

    /* 1) Create a new pipe; pipe_id is filled in by PipeInit. */
    if (PipeInit(&pipe_id) < 0) {
        TracePrintf(0, "PipeInit failed\n");
        exit(1);
    }

    /* 2) Fork a child. */
    pid = Fork();
    if (pid < 0) {
        TracePrintf(0, "Fork failed\n");
        exit(1);
    }

    if (pid == 0) {
        /* --- Child process: read from the pipe and print what it got. --- */
        char buf[BUF_SIZE];
        int nbytes;

        /* Block until parent writes something into the pipe. */
        nbytes = PipeRead(pipe_id, buf, BUF_SIZE - 1);
        if (nbytes < 0) {
            TracePrintf(0, "Child: PipeRead failed\n");
            Exit(1);
        }

        /* Null‐terminate and print. */
        buf[nbytes] = '\0';
        TracePrintf(0, "Child (pid %d) read %d bytes: \"%s\"\n", GetPid(), nbytes, buf);
        Exit(0);
    } else {
        /* --- Parent process: write a message into the pipe, then wait. --- */
        const char *msg = "Hello from parent!";
        int msglen = strlen(msg);

        /* Give the child a moment (optional); pipes block anyway, so not strictly needed. */
        Delay(2);

        if (PipeWrite(pipe_id, (void *)msg, msglen) != msglen) {
            TracePrintf(0, "Parent: PipeWrite failed\n");
            Exit(1);
        }

        /* Wait for child to finish. */
        Wait(&status);
        TracePrintf(0, "Parent: child exited with status %d\n", status);
    }

    return 0;
}

