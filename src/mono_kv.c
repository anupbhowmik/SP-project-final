#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "kv_backend.h"
#include "sim_config.h"

/*
 * Tracks the state of a single sequence implementing a monolithic backend
 * 
 * max_tokens: number of tokens this sequence can have
 * cur_tokens: number of tokens this sequence already has
 * bytes_per_token: size of the KV data (size K + size V)
 * kv_buffer: buffer that holds all data for this sequence
 */
typedef struct MonoSeqState {
    size_t max_tokens;
    size_t cur_tokens;
    size_t bytes_per_token;
    unsigned char* kv_buffer;
} MonoSeqState;

/*
 * Implementation state for the monolithic KV backend
 * 
 * cfg: config params
 * seqs: all sequence states
 * num_seqs: current number of active sequences
 * capacity: current allocated capacity of the seqs array
 * mutex: mutex lock for sequence management
 */
typedef struct MonoKVImpl {
    SimConfig cfg;
    MonoSeqState* seqs;
    size_t num_seqs;
    size_t capacity;
    pthread_mutex_t mutex;
} MonoKVImpl;

/*
 * Initialize a new sequence in the KV cache
 * 
 * allocates a MonoSeqState and a buffer for the data.
 * the buffer is sized according to the config.
 * 
 * returns The ID of the new sequence
 */
static SeqId mono_init_sequence(KVBackend* backend, const SequenceWork* work) {
    MonoKVImpl* impl = (MonoKVImpl*) backend->impl;
    pthread_mutex_lock(&impl->mutex);

    // Dynamically grow the sequence array if needed
    if (impl->num_seqs == impl->capacity) {
        size_t new_cap = impl->capacity == 0 ? 16 : impl->capacity * 2;
        MonoSeqState* ns = (MonoSeqState*) realloc(impl->seqs, new_cap * sizeof(MonoSeqState));
        if (!ns) {
            pthread_mutex_unlock(&impl->mutex);
            abort();
        }
        impl->seqs = ns;
        impl->capacity = new_cap;
    }

    // Assign a new sequence ID and initialize its state
    SeqId id = impl->num_seqs++;
    MonoSeqState* s = &impl->seqs[id];
    s->bytes_per_token = bytes_per_token(&impl->cfg);
    s->max_tokens = s->max_tokens; 
    
    s->cur_tokens = 0;
    s->kv_buffer = (unsigned char*) malloc(s->max_tokens * s->bytes_per_token);
    if (!s->kv_buffer) {
        pthread_mutex_unlock(&impl->mutex);
        abort();
    }

    pthread_mutex_unlock(&impl->mutex);
    return id;
}

/*
 * add a new token to a sequence's KV cache
 * 
 * increment the token count for the given sequence if long as we haven't
 * exceeded the maximum context length.
 */
static void mono_append_token(KVBackend* backend, SeqId id) {
    MonoKVImpl* impl = (MonoKVImpl*) backend->impl;
    MonoSeqState* s = &impl->seqs[id];
    
    if (s->cur_tokens < s->max_tokens) {
        s->cur_tokens++;
    }
}

/*
 * Mark a sequence as complete
 * 
 * this does nothing in the current implementation. 
 * In a real system, this might deallocate buffers or update
 * memory accounting when a sequence finishes.
 */
static void mono_finish_sequence(KVBackend* backend, SeqId id) {
    (void) backend;
    (void) id;
}

/*
 * collect and return memory usage statistics
 * 
 * logical_tokens: Total number of tokens actually stored across all sequences
 * physical_bytes: Total memory allocated (may be higher than used due to pre-allocation)
 * logical_bytes: Total bytes used for actual token data
 */
static KVStats mono_stats(KVBackend* backend) {
    MonoKVImpl* impl = (MonoKVImpl*) backend->impl;
    KVStats st = {0, 0, 0};

    pthread_mutex_lock(&impl->mutex);
    for (size_t i = 0; i < impl->num_seqs; ++i) {
        MonoSeqState* s = &impl->seqs[i];
        st.logical_tokens += s->cur_tokens;
        st.physical_bytes += s->max_tokens * s->bytes_per_token;
    }
    pthread_mutex_unlock(&impl->mutex);
    st.logical_bytes = st.logical_tokens * bytes_per_token(&impl->cfg);
    return st;
}

/*
 * clean up and deallocate all resources
 * 
 * frees all allocated data.
 */
static void mono_destroy(KVBackend* backend) {
    MonoKVImpl* impl = (MonoKVImpl*) backend->impl;
    
    for (size_t i = 0; i < impl->num_seqs; ++i) {
        free(impl->seqs[i].kv_buffer);
    }
    
    free(impl->seqs);
    pthread_mutex_destroy(&impl->mutex);
    free(impl);
    backend->impl = NULL;
}

static const KVBackendVTable MONO_VTABLE = {
    .init_sequence   = mono_init_sequence,
    .append_token    = mono_append_token,
    .finish_sequence = mono_finish_sequence,
    .stats           = mono_stats,
    .destroy         = mono_destroy
};

/*
 * factory function to create a monolithic KV backend
 */
KVBackend* create_monolithic_backend(const SimConfig* cfg) {
    // Allocate and initialize the implementation state
    MonoKVImpl* impl = (MonoKVImpl*) calloc(1, sizeof(MonoKVImpl));
    impl->cfg = *cfg;
    pthread_mutex_init(&impl->mutex, NULL);
    
    // allocate the sequence array based on expected sequences
    impl->capacity = cfg->num_sequences;
    impl->seqs = (MonoSeqState*) calloc(impl->capacity, sizeof(MonoSeqState));
    
    // create the backend interface and link it to the implementation
    KVBackend* b = (KVBackend*) calloc(1, sizeof(KVBackend));
    b->impl = impl;
    b->vtable = &MONO_VTABLE;
    
    return b;
}