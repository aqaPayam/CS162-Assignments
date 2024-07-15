#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

s_block_ptr heap_start = NULL;


#include "mm_alloc.h"

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


void *allocate_block(s_block_ptr block, size_t size) {
    split_block(block, size);
    block->is_free = 0;
    return block->ptr;
}

void *allocate_new_block(size_t size, s_block_ptr last_block) {
    return extend_heap(last_block, size);
}

void *realloc_existing_block(void *ptr, size_t size, s_block_ptr block) {
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    size_t size_to_copy = size <= block->size ? size : block->size;
    if (size > size_to_copy) {
        memset(new_ptr, 0, size);
    }
    memcpy(new_ptr, block->ptr, size_to_copy);
    mm_free(block->ptr);
    return new_ptr;
}

void *mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    s_block_ptr current_block = heap_start;
    s_block_ptr last_block = NULL;

    while (current_block != NULL) {
        if (current_block->is_free && current_block->size >= size) {
            return allocate_block(current_block, size);
        }
        last_block = current_block;
        current_block = current_block->next;
    }

    return allocate_new_block(size, last_block);
}

void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    s_block_ptr block = get_block(ptr);
    if (block == NULL) {
        return NULL;
    }

    return realloc_existing_block(ptr, size, block);
}

void mm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    s_block_ptr block = get_block(ptr);
    if (block == NULL) {
        return;
    }

    block->is_free = 1;
    memset(block->ptr, 0, block->size);
    fusion(block);
}


s_block_ptr create_block(void *ptr, size_t size) {
    s_block_ptr new_block = (s_block_ptr) ptr;
    new_block->size = size;
    new_block->is_free = 1;
    new_block->next = NULL;
    new_block->prev = NULL;
    new_block->ptr = ptr + BLOCK_SIZE;
    memset(new_block->ptr, 0, size);
    return new_block;
}

void insert_block_after(s_block_ptr prev_block, s_block_ptr new_block) {
    if (prev_block != NULL) {
        new_block->next = prev_block->next;
        prev_block->next = new_block;
    } else {
        heap_start = new_block;
    }

    if (new_block->next != NULL) {
        new_block->next->prev = new_block;
    }
    new_block->prev = prev_block;
}

void update_block(s_block_ptr block, size_t size) {
    block->size = size;
}

void initialize_block(s_block_ptr block, s_block_ptr prev_block, size_t size) {
    block->prev = prev_block;
    block->next = NULL;
    block->is_free = 0;
    block->size = size;
    block->ptr = block + 1;
    memset(block->ptr, 0, size);
}

void try_fusion_with_previous(s_block_ptr b) {
    if (b->prev != NULL && b->prev->is_free) {
        b->prev->size += b->size + BLOCK_SIZE;
        b->prev->next = b->next;

        if (b->next != NULL) {
            b->next->prev = b->prev;
        }
        b = b->prev;
    }
}

void try_fusion_with_next(s_block_ptr b) {
    if (b->next != NULL && b->next->is_free) {
        b->size += b->next->size + BLOCK_SIZE;
        b->next = b->next->next;

        if (b->next != NULL) {
            b->next->prev = b;
        }
    }
}

void split_block(s_block_ptr b, size_t s) {
    if (b->size > s + BLOCK_SIZE) {
        s_block_ptr new_block = create_block(b->ptr + s, b->size - s - BLOCK_SIZE);
        insert_block_after(b, new_block);
        update_block(b, s);

        if (b->next != NULL) {
            b->next->prev = new_block;
        }
    }
}

void *extend_heap(s_block_ptr last, size_t s) {
    s_block_ptr new_block = (s_block_ptr) sbrk(s + BLOCK_SIZE);
    if (new_block == (void *) -1) {
        return NULL;
    }

    insert_block_after(last, new_block);
    initialize_block(new_block, last, s);
    return new_block->ptr;
}

s_block_ptr get_block(void *p) {
    s_block_ptr current_block = heap_start;
    while (current_block != NULL) {
        if (current_block->ptr == p) {
            return current_block;
        }
        current_block = current_block->next;
    }
    return NULL;
}

void fusion(s_block_ptr b) {
    try_fusion_with_previous(b);
    try_fusion_with_next(b);
}
