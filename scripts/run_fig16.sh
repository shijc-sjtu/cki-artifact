#!/bin/bash -e

. scripts/parse_config.sh

# Run redis benchmark with different container runtimes
./scripts/run_fig16.exp cki cki ./runpk_test.sh $qemu $password $container_ip $server_ip redis              # CKI
./scripts/run_fig16.exp pvm pvm ./pvm_test_hostnet.sh $qemu $password $container_ip $server_ip redis        # PVM
./scripts/run_fig16.exp pvm hvm ./hvm_test_hostnet.sh $qemu $password $container_ip $server_ip redis        # HVM

# Run redis benchmark with different container runtimes
./scripts/run_fig16.exp cki cki ./runpk_test.sh $qemu $password $container_ip $server_ip memcached          # CKI
./scripts/run_fig16.exp pvm pvm ./pvm_test_hostnet.sh $qemu $password $container_ip $server_ip memcached    # PVM
./scripts/run_fig16.exp pvm hvm ./hvm_test_hostnet.sh $qemu $password $container_ip $server_ip memcached    # HVM

# Collect throughputs from the log files and generate Figure 16
./scripts/plot_fig16.py
