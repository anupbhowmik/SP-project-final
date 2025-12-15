#include <stdlib.h>
#include "sim_config.h"
#include "workload.h"

/*
 * create a synthetic LLM inference workload
 * 
 * workload parameters include
 * - shared_prompt_setup: sequences can be grouped to share common prefixes
 * - prompt_length: base shared prefix + random extra tokens
 * - generation length: random number of tokens each sequence will generate
 * 
 * cfg: Configuration specifying number of sequences, prompt/generation lengths, etc.
 * 
 * returns an array of SequenceWork structures, one per sequence
 */
SequenceWork* generate_workload(const SimConfig* cfg) {
    SequenceWork* w = calloc(cfg->num_sequences, sizeof(SequenceWork));
    if (!w) {
        abort();
    }

    // Ensure tokens_per_page is positive
    size_t tokens_per_page = cfg->tokens_per_page ? cfg->tokens_per_page : 1;

    // Calculate a page-aligned shared prefix length
    // Base prefix = 128 pages, rounded to nearest page boundary
    size_t base_prefix = tokens_per_page * 128;
    size_t shareable_prefix = (base_prefix / tokens_per_page) * tokens_per_page;

    /* assign workload parameters to each sequence
     * group assignment, shared prefix length, total prompt length, gen length */
    for (size_t i = 0; i < cfg->num_sequences; ++i) {
        // Assign sequence to a group
        int group = cfg->num_groups ? (int)(i % cfg->num_groups) : -1;
        w[i].shared_prompt_id = group;
        w[i].shared_prompt_tokens = (group >= 0) ? shareable_prefix : 0;

        // add random extra tokens on top of the shared prefix
        size_t extra_prompt = rand() % (cfg->max_prompt_extra + 1);
        size_t prompt_base = (group >= 0) ? w[i].shared_prompt_tokens : 0;
        w[i].prompt_tokens = prompt_base + extra_prompt;

        // generate a random amout of output tokens
        size_t gen_span = cfg->max_gen_tokens - cfg->min_gen_tokens + 1;
        w[i].gen_tokens = cfg->min_gen_tokens + (rand() % gen_span);
    }
    return w;
}