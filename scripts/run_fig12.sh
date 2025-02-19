#!/bin/bash -e

. scripts/parse_config.sh

# Run the page-fault-intensive applications with different container runtimes
./scripts/run_fig12.exp cki cki ./runpk_test.sh $qemu $password # CKI
./scripts/run_fig12.exp pvm pvm ./pvm_test.sh $qemu $password   # PVM
./scripts/run_fig12.exp pvm hvm ./hvm_test.sh $qemu $password   # HVM

# Collect latencies from the log files and generate Figure 12
./scripts/plot_fig12.py
