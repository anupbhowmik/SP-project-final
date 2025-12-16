#define _GNU_SOURCE 1
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "sim_config.h"
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef struct Page {
    unsigned char* base;
    unsigned int ref;
} Page;

typedef struct PageAllocator {
    unsigned char* arena;
    size_t page_bytes;
    size_t num_pages;
    Page*  pages;

    Page** free_list;
    size_t free_count;
    size_t free_capacity;

    pthread_mutex_t mutex;
} PageAllocator;

/*
 * page_allocator_create
 *
 * Creates a fixed-size page pool used by the paged KV backend.
 * - Derives page size from cfg->tokens_per_page * bytes_per_token(cfg).
 * - Splits a single mmap'd arena into pa->num_pages fixed-size pages.
 * - Initializes metadata (Page array) and a LIFO free list of all pages.
 * - Initializes the allocator mutex.
 *
 * Returns: a fully initialized PageAllocator*; aborts on allocation/mmap failure.
 */
PageAllocator* page_allocator_create(const SimConfig* cfg) {
    PageAllocator* pa = (PageAllocator*) calloc(1, sizeof(PageAllocator));
    if (!pa) abort();

    pa->page_bytes = cfg->tokens_per_page * bytes_per_token(cfg);
    pa->num_pages  = cfg->arena_bytes / pa->page_bytes;

    size_t arena_size = pa->num_pages * pa->page_bytes;
    pa->arena = (unsigned char*) mmap(NULL, arena_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pa->arena == MAP_FAILED) {
        free(pa);
        abort();
    }

    pa->pages = (Page*) malloc(pa->num_pages * sizeof(Page));
    pa->free_list = (Page**) malloc(pa->num_pages * sizeof(Page*));
    pa->free_capacity = pa->num_pages;
    pa->free_count = 0;

    for (size_t i = 0; i < pa->num_pages; ++i) {
        pa->pages[i].base = pa->arena + i * pa->page_bytes;
        pa->pages[i].ref  = 0;
        pa->free_list[pa->free_count++] = &pa->pages[i];
    }

    pthread_mutex_init(&pa->mutex, NULL);
    return pa;
}

/*
 * page_allocator_destroy
 *
 * Releases all resources owned by the allocator:
 * - Unmaps the arena
 * - Frees page metadata and the free list
 * - Destroys the mutex
 *
 * Precondition: no other threads are using the allocator.
 */
void page_allocator_destroy(PageAllocator* pa) {
    size_t arena_size = pa->num_pages * pa->page_bytes;
    munmap(pa->arena, arena_size);
    free(pa->pages);
    free(pa->free_list);
    pthread_mutex_destroy(&pa->mutex);
    free(pa);
}

/*
 * page_alloc
 *
 * Allocates one page from the allocator free list:
 * - Pops one Page* from free_list (LIFO)
 * - Sets its refcount to 1
 *
 * Thread-safety: protected by pa->mutex.
 * Failure: aborts if the free list is empty (simulator out-of-memory).
 */
Page* page_alloc(PageAllocator* pa) {
    pthread_mutex_lock(&pa->mutex);
    if (pa->free_count == 0) {
        pthread_mutex_unlock(&pa->mutex);
        abort(); // out of pages
    }
    Page* p = pa->free_list[--pa->free_count];
    p->ref = 1;
    pthread_mutex_unlock(&pa->mutex);
    return p;
}

/*
 * page_inc_ref
 *
 * Increments the reference count for a page.
 * Used when multiple sequences share the same underlying page (prefix sharing).
 *
 * Note: This simulator increments without atomic ops or a mutex; correctness relies
 * on callers avoiding concurrent increments to the same Page from multiple threads.
 */
void page_inc_ref(PageAllocator* pa, Page* p) {
    (void) pa;
    // for a simulator, we can just increment without atomic
    p->ref++;
}

/*
 * page_dec_ref
 *
 * Decrements the reference count for a page.
 * When the count reaches zero, returns the page back to the allocator free list.
 *
 * Thread-safety: uses pa->mutex to protect the free list and refcount-to-free transition.
 * Failure: aborts if refcount is already zero (double-free / bug).
 */
void page_dec_ref(PageAllocator* pa, Page* p) {
    pthread_mutex_lock(&pa->mutex);
    if (p->ref == 0) {
        pthread_mutex_unlock(&pa->mutex);
        abort();
    }
    p->ref--;
    if (p->ref == 0) {
        pa->free_list[pa->free_count++] = p;
    }
    pthread_mutex_unlock(&pa->mutex);
}

/*
 * page_allocator_pages_in_use
 *
 * Counts how many pages currently have refcount > 0.
 * This is used by the paged backend stats to compute physical_bytes as:
 *   pages_in_use * page_bytes
 *
 * Thread-safety: holds pa->mutex while scanning page metadata.
 */
size_t page_allocator_pages_in_use(PageAllocator* pa) {
    size_t used = 0;
    pthread_mutex_lock(&pa->mutex);
    for (size_t i = 0; i < pa->num_pages; ++i) {
        if (pa->pages[i].ref > 0) used++;
    }
    pthread_mutex_unlock(&pa->mutex);
    return used;
}

/*
 * page_allocator_page_bytes
 *
 * Returns the page size in bytes (tokens_per_page * bytes_per_token).
 * Used for memory accounting in stats.
 */
size_t page_allocator_page_bytes(PageAllocator* pa) {
    return pa->page_bytes;
}
