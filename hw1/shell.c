#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

int cmd_quit(tok_t arg[]);

int cmd_help(tok_t arg[]);

int cmd_cd(tok_t arg[]);

int cmd_pwd(tok_t arg[]);

int cmd_wait(tok_t arg[]);

/* Command Lookup table */
typedef int cmd_fun_t(tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
    cmd_fun_t *fun;
    char *cmd;
    char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
        {cmd_help, "?",    "show this help menu"},
        {cmd_quit, "quit", "quit the command shell"},
        {cmd_cd,   "cd",   "change directory"},
        {cmd_pwd,  "pwd",  "prints the current working directory path"},
        {cmd_wait, "wait", "makes a shell script or terminal session wait for background processes to finish."},
};

int cmd_help(tok_t arg[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    }
    return 1;
}

int cmd_quit(tok_t arg[]) {
    printf("Bye\n");
    exit(0);
    return 1;
}

int cmd_cd(tok_t arg[]) {
    //using chdir : a system function (system call) that is used to change the current working directory.
    int status_code = chdir(arg[0]);
    if (status_code != 0) {
        printf("the path does not exist or not a directory: %s\n", arg[0]);
        return 1;
    }
    //printf("path change successfully.\n");
    return 0;
}

int cmd_pwd(tok_t arg[]) {
    //using getcwd : The getcwd() function shall place an absolute pathname
    // of the current working directory in the array pointed to by buf, and return buf
    char *buffer = (char *) malloc(MAX_PATH_LENGTH * sizeof(char));
    char *result = getcwd(buffer, MAX_PATH_LENGTH);
    if (result != NULL) {
        printf("%s\n", buffer);
        return 0;
    }
    //printf("unknown error in pwd --for debug--\n");
    return 1;
}

int cmd_wait(tok_t arg[]) {
    while (waitpid(-1, NULL, 0) > 0) { ; }
    //printf("waiting finished.\n");
    return 1;
}


int lookup(char cmd[]) {
    int i;
    for (i = 0; i < (sizeof(cmd_table) / sizeof(fun_desc_t)); i++) {
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
    }
    return -1;
}

void init_shell() {
    /* Check if we are running interactively */
    shell_terminal = STDIN_FILENO;

    /** Note that we cannot take control of the terminal if the shell
        is not interactive */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {

        /* force into foreground */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        shell_pgid = getpid();
        /* Put shell in its own process group */
        if (setpgid(shell_pgid, shell_pgid) < 0) {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);
        tcgetattr(shell_terminal, &shell_tmodes);

        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
    }

    //create root Process
    first_process = create_process(NULL);
    first_process->pid = getpid();
}


int shell(int argc, char *argv[]) {
    char *s = malloc(INPUT_STRING_SIZE + 1);            /* user input string */
    tok_t *t;            /* tokens parsed from input */
    int fundex = -1;


    init_shell();

    // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);


    while ((s = freadln(stdin))) {
        t = getToks(s); /* break the line into tokens */
        fundex = lookup(t[0]); /* Is first token a shell literal */
        if (fundex >= 0) cmd_table[fundex].fun(&t[1]);
        else if (t[0] != NULL) {
            run_process(t);
        } else {
            //fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
        }
    }
    return 0;
}
