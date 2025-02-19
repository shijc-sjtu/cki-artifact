#!/bin/bash -e

. scripts/parse_config.sh

# Run sqlite-bench with different container runtimes
./scripts/run_fig14.exp cki cki ./runpk_test.sh $qemu $password # CKI
./scripts/run_fig14.exp pvm pvm ./pvm_test.sh $qemu $password   # PVM
./scripts/run_fig14.exp pvm hvm ./hvm_test.sh $qemu $password   # HVM

# Collect throughputs from the log files and generate Figure 14
./scripts/plot_fig14.py
