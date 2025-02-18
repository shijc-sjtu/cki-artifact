#!/bin/bash -e

if [[ $1 == "" ]]; then
        echo usage: ./runpk_test.sh app
        exit 1
fi

insmod ../pkm.ko &>/dev/null || true

# sudo nerdctl container prune -f
# sudo nerdctl rm -f tester-runpk &>/dev/null || true
# sudo rm -rf /var/lib/nerdctl/1935db59/names/default/tester-runpk &>/dev/null || true

./create_taps.sh 1
# ./init_vfio_nic.sh
# ip -s -s neigh flush all

echo "6" > /proc/sys/kernel/printk

# sudo nerdctl run --privileged --cpuset-cpus 0 -it --rm \
#         -v $(realpath $(pwd)/..):/root/split-kernel -w /root/split-kernel/tender \
#         -p 9101:9101/udp -p 6379:6379/tcp -p 11211:11211/tcp -p 8080:8080/tcp \
#         --name tester-runpk busybox:latest ./launch.sh
subcmd=${2:-default}
[[ $3 ]] && subcmd=${subcmd},$3
[[ $4 ]] && subcmd=${subcmd},$4
./cki_launcher.sh $1 $subcmd $IP

echo "4" > /proc/sys/kernel/printk
