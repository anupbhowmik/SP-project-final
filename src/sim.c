#define _DEFAULT_SOURCE // Required for usleep

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // Required for usleep
#include <pthread.h>
#include "sim.h"
#include "kv_backend.h"
#include "workload.h"

typedef struct ThreadArgs {
    KVBackend* backend;
    const SimConfig* cfg;
    const SequenceWork* work;
    size_t index;
} ThreadArgs;

static void* decode_thread(void* arg) {
    ThreadArgs* a = (ThreadArgs*) arg;
    const SequenceWork* w = &a->work[a->index];

    SeqId id = kv_init_sequence(a->backend, w);

    // Prompt
    for (size_t t = 0; t < w->prompt_tokens; ++t) {
        kv_append_token(a->backend, id);
        if (a->cfg->enable_sleep) {
            usleep(100); // 0.1 ms simulating time per token generation
        }
    }
    // Decode
    for (size_t t = 0; t < w->gen_tokens; ++t) {
        kv_append_token(a->backend, id);
        if (a->cfg->enable_sleep) {
            usleep(100);
        }
    }
    
    return NULL;
}

KVStats run_simulation(KVBackend* backend,
                       const SimConfig* cfg,
                       const SequenceWork* work) {
    size_t n = cfg->num_sequences;
    pthread_t* threads = (pthread_t*) malloc(n * sizeof(pthread_t));
    ThreadArgs* args   = (ThreadArgs*) malloc(n * sizeof(ThreadArgs));

    for (size_t i = 0; i < n; ++i) {
        args[i].backend = backend;
        args[i].cfg     = cfg;
        args[i].work    = work;
        args[i].index   = i;
        pthread_create(&threads[i], NULL, decode_thread, &args[i]);
    }

    for (size_t i = 0; i < n; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);

    return kv_stats(backend);
}
