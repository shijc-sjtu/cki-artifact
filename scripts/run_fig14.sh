#!/bin/bash -e

. scripts/parse_config.sh

./scripts/run_fig14.exp cki cki ./runpk_test.sh $qemu $password
./scripts/run_fig14.exp pvm pvm ./pvm_test.sh $qemu $password
./scripts/run_fig14.exp pvm hvm ./hvm_test.sh $qemu $password

./scripts/plot_fig14.py
