#!/bin/bash -e

. scripts/parse_config.sh

./scripts/run_fig12.exp cki cki ./runpk_test.sh $qemu $password
./scripts/run_fig12.exp pvm pvm ./pvm_test.sh $qemu $password
./scripts/run_fig12.exp pvm hvm ./hvm_test.sh $qemu $password

./scripts/plot_fig12.py
