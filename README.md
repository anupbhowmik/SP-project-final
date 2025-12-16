# Systems Programming (CS 551) Final Project

This repository contains the final project for the CS 551 Systems Programming. We tried to simulate memory allocations in LLM inference. Here, monolithic backend pre-allocates context window per sequence, while paged backend allocates pages lazily and deduplicates shared-prefix pages across sequences to save memory.

## How to Run this Project

```bash
make clean
make
./llm_sim
```

Sample Simulation Results:

```bash
bytes_per_token = 8192

Monolithic:
  logical_bytes  = 1780080640
  physical_bytes = 2147483648
  waste_bytes    = 367403008 (17.11%)
  memory_saved   = 0 (0.00%)

Paged+Prefix:
  logical_bytes  = 1780080640
  physical_bytes = 746323968
  waste_bytes    = 0 (0.00%)
  memory_saved   = 1033756672 (58.07%)
```

## Report

Project details and the key features implemented can be found in [report.txt](./report.txt).

## Team mates

1. Anup Bhowmik
2. Evan Dreher
3. Tawheed Islam Bhuian
