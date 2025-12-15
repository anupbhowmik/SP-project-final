# Systems Programming (CS 551) Final Project

This repository contains the final project for the CS 551 Systems Programming. We tried to simulate memory allocations in LLM inference. Here, monolithic backend pre-allocates context window per sequence, while paged allocates pages lazily and deduplicates shared-prefix pages across sequences to save memory.


How to run this project:

We have included the executable.
> ./llm_sim 
> ./llm_sim
bytes_per_token = 8192
Monolithic:
  logical_bytes  = 1783947264
  physical_bytes = 2147483648
  waste_bytes    = 363536384 (16.93%)
Paged+Prefix:
  logical_bytes  = 1783947264
  physical_bytes = 749600768
  memory_saved   = 1034346496 (57.98%)


For build and run, 
> make clean 
> make 
> ./llm_sim


Team mates: 
    1. Anup Bhowmik 
    2. Evan Dreher 
    3. Tawheed Islam Bhuian 
