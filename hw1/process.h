#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "parse.h"

typedef struct process {
    char **argv;
    int argc;
    pid_t pid;
    char completed;
    char stopped;
    char background;
    int status;
    struct termios tmodes;
    int stdin, stdout, stderr;
    struct process *next;
    struct process *prev;
} process;

process *first_process; //pointer to the first process that is launched */

process *create_process(tok_t *inputString);

void run_process(tok_t *t);


#endif
