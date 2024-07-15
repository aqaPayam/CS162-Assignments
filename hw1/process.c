#include "process.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#include "parse.h"

void set_argvs(process *p, int redirectIndex) {
    int i;
    for (i = redirectIndex; i < p->argc; i++)
        p->argv[i] = NULL;
}

process *initial_process(tok_t *inputString) {
    process *p = (process *) malloc(sizeof(process));
    p->argv = inputString;
    p->argc = tokenLength(inputString);
    p->completed = 0;
    p->stopped = 0;
    p->status = 0;
    p->stdin = 0;
    p->stdout = 1;
    p->stderr = 2;
    return p;
}

void handle_std(process *p) {
    int inredirect = isDirectTok(p->argv, "<");
    int outredirect = isDirectTok(p->argv, ">");

    if (outredirect >= 0) {
        if (p->argv[outredirect + 1] != NULL) {
            int file = open(p->argv[outredirect + 1], O_CREAT | O_TRUNC | O_WRONLY,
                            S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
            if (file >= 0)
                p->stdout = file;
            set_argvs(p, outredirect);
        }
    }

    if (inredirect >= 0) {
        if (p->argv[inredirect + 1] != NULL) {
            int file = open(p->argv[inredirect + 1], O_RDONLY);
            if (file >= 0)
                p->stdin = file;
            set_argvs(p, inredirect);
        }
    }
}

void set_background_foreground(process *p) {
    if (p->argv && strcmp(p->argv[p->argc - 1], "&") == 0) {
        p->background = 1;
        p->argv[--p->argc] = NULL;
    }
}

process *create_process(tok_t *inputString) {
    process *p = initial_process(inputString);
    if (p->argv)
        handle_std(p);

    p->argc = tokenLength(p->argv);
    p->prev = NULL;
    p->next = NULL;
    set_background_foreground(p);
    return p;
}

void add_process(process *p) {
    process *prev = first_process;
    while (prev->next)
        prev = prev->next;
    prev->next = p;
    p->prev = prev;
}


void launch_process(process *p) {
    tok_t *inputPath = p->argv;
    tok_t *paths = getToks(getenv("PATH"));
    int pathsLen = tokenLength(paths);

    dup2(p->stdin, 0);
    dup2(p->stdout, 1);

    if (access(p->argv[0], F_OK) == 0) {
        execv(p->argv[0], &inputPath[0]);
        return;
    }

    char *path = (char *) malloc(MAX_PATH_LENGTH * sizeof(char));

    for (int i = 0; i < pathsLen; i++) {
        strcpy(path, paths[i]);
        strcat(path, "/");
        strcat(path, inputPath[0]);
        if (access(path, F_OK) == 0) {
            execv(path, &inputPath[0]);
            free(path);
            return;
        }

    }
    free(path);
    fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
}


void run_process(tok_t *t) {
    process *process = create_process(t);
    add_process(process);

    int cpid = fork();
    if (cpid > 0) {
        //parent
        process->pid = getpid();
        setpgid(process->pid, process->pid);
        if (!process->background) {
            tcsetpgrp(STDIN_FILENO, process->pid);
            waitpid(cpid, NULL, 2);
        }

    } else if (cpid == 0) {
        //child
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        process->pid = getpid();
        launch_process(process);

    }
}

