#!/bin/bash -e

. scripts/parse_config.sh

./scripts/run_fig16.exp cki cki ./runpk_test.sh $qemu $password $container_ip $server_ip redis
./scripts/run_fig16.exp pvm pvm ./pvm_test_hostnet.sh $qemu $password $container_ip $server_ip redis
./scripts/run_fig16.exp pvm hvm ./hvm_test_hostnet.sh $qemu $password $container_ip $server_ip redis

./scripts/run_fig16.exp cki cki ./runpk_test.sh $qemu $password $container_ip $server_ip memcached
./scripts/run_fig16.exp pvm pvm ./pvm_test_hostnet.sh $qemu $password $container_ip $server_ip memcached
./scripts/run_fig16.exp pvm hvm ./hvm_test_hostnet.sh $qemu $password $container_ip $server_ip memcached

./scripts/plot_fig16.py
