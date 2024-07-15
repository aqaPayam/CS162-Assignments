/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    int *data;
    int *data2;
    int *data3;

    data = (int*) mm_malloc(8);
    data2 = (int*) mm_malloc(24);
    data3 = (int*) mm_malloc(24);
    data[0] = 1;
    data[1] = 32;
    mm_free(data);
    mm_free(data3);
    mm_free(data2);
    printf("malloc sanity test successful!\n");
    return 0;
}
