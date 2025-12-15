# Systems Programming (CS 551) Final Project

This repository contains the final project for the CS 551 Systems Programming. We tried to simulate memory allocations in LLM inference. Here, monolithic backend pre-allocates context window per sequence, while paged allocates pages lazily and deduplicates shared-prefix pages across sequences to save memory.


How to run this project:

We have included the executable.
> ./llm_sim 

For build and run, 
> make clean 
> make 
> ./llm_sim


Team mates: 
    1. Anup Bhowmik 
    2. Evan Dreher 
    3. Tawheed Islam Bhuian 
