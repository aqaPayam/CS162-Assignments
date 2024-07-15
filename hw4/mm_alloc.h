#pragma once

#ifndef _malloc_H_
#define _malloc_H_

/* Define the block size since the sizeof will be wrong */
#define BLOCK_SIZE 40

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

typedef struct s_block *s_block_ptr;

/* block struct */
struct s_block {
    size_t size;
    struct s_block *next;
    struct s_block *prev;
    int is_free;
    void *ptr;
    /* A pointer to the allocated block */
    char data[0];
};

// Helper functions
void *allocate_block(s_block_ptr block, size_t size);

void *allocate_new_block(size_t size, s_block_ptr last_block);

void *realloc_existing_block(void *ptr, size_t size, s_block_ptr block);

// Main functions
void *mm_malloc(size_t size);

void *mm_realloc(void *ptr, size_t size);

void mm_free(void *ptr);

// Block management functions
s_block_ptr create_block(void *ptr, size_t size);

void insert_block_after(s_block_ptr prev_block, s_block_ptr new_block);

void update_block(s_block_ptr block, size_t size);

void initialize_block(s_block_ptr block, s_block_ptr prev_block, size_t size);

void try_fusion_with_previous(s_block_ptr b);

void try_fusion_with_next(s_block_ptr b);

void split_block(s_block_ptr b, size_t s);

void *extend_heap(s_block_ptr last, size_t s);

s_block_ptr get_block(void *p);

void fusion(s_block_ptr b);

#ifdef __cplusplus
}
#endif

#endif
