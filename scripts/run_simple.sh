#!/bin/bash -e

. scripts/parse_config.sh
./scripts/run_simple.exp $password $qemu
