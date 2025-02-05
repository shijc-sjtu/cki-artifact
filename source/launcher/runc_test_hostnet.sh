#!/bin/bash -e

if [[ $1 == "" ]]; then
        echo usage: ./runc_test.sh app
        exit 1
fi

sudo nerdctl container prune -f
sudo nerdctl rm -f tester-runc &>/dev/null || true
sudo rm -rf /var/lib/nerdctl/1935db59/names/default/tester-runc &>/dev/null || true

sudo nerdctl run --privileged --cpuset-cpus 0 -it --rm --tmpfs /tmp --net=host \
        --name tester-runc --insecure-registry 192.168.12.125:5000/split_kernel_$1:latest /bin/command-${2:-default} ${@:3}
