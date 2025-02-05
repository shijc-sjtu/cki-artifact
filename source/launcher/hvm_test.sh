#!/bin/bash -e

if [[ $1 == "" ]]; then
        echo usage: ./hvm_test.sh app
        exit 1
fi

sudo rmmod kvm-pvm &>/dev/null || true
sudo modprobe kvm-amd &>/dev/null || true
sudo cp /opt/kata/share/defaults/kata-containers/configuration-hvm.toml /opt/kata/share/defaults/kata-containers/configuration.toml 

sudo nerdctl container prune -f
sudo nerdctl rm -f tester-hvm &>/dev/null || true
sudo rm -rf /var/lib/nerdctl/1935db59/names/default/tester-hvm &>/dev/null || true

sudo nerdctl run --name tester-hvm --runtime "io.containerd.kata.v2" -it --rm --tmpfs /tmp \
        -p 6379:6379/tcp -p 11211:11211/tcp -p 8080:8080/tcp \
        --insecure-registry 192.168.12.125:5000/split_kernel_$1:latest /bin/command-${2:-default} ${@:3}

