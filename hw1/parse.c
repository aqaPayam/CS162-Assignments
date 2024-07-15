#include <string.h>
#include <stdlib.h>
#include "parse.h"

/*          Get tokens from a line of characters */
/* Return:  new array of pointers to tokens */
/* Effects: token separators in line are replaced with NULL */
/* Storage: Resulting token array points into original line */

#define TOKseparator " \n:"

tok_t *getToks(char *line) {
    int i;
    char *c;

    tok_t *toks = malloc(MAXTOKS * sizeof(tok_t));
    for (i = 0; i < MAXTOKS; i++) toks[i] = NULL;     /* empty token array */


    c = strtok(line, TOKseparator);     /* Start tokenizer on line */
    for (i = 0; c && (i < MAXTOKS); i++) {
        toks[i] = c;
        c = strtok(NULL, TOKseparator);    /* scan for next token */
    }
    return toks;
}

/* Locate special processing character */
int isDirectTok(tok_t *t, char *R) {
    int i;
    for (i = 0; i < MAXTOKS - 1 && t[i]; i++) {
        if (strncmp(t[i], R, 1) == 0) return i;
    }
    return -1;
}

//new added
int tokenLength(tok_t *toks) {
    if (toks == NULL)
        return 0;
    for (int i = 0; i < MAXTOKS; i++)
        if (!toks[i])
            return i;
    return 0;
}
