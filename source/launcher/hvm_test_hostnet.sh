#!/bin/bash -e

if [[ $1 == "" ]]; then
        echo usage: ./hvm_test_hostnet.sh app
        exit 1
fi

./create_taps.sh 1

app=$1

subcmd=${2:-default}
[[ $3 ]] && subcmd=${subcmd},$3
[[ $4 ]] && subcmd=${subcmd},$4

sudo rmmod kvm-pvm &>/dev/null || true
sudo modprobe kvm-amd &>/dev/null || true

sudo /opt/kata/bin/qemu-system-x86_64 \
        -enable-kvm -nographic \
        -machine q35,accel=kvm,nvdimm=on \
        -cpu host,pmu=off \
        -m 2048M,slots=10,maxmem=128939M \
        -smp 1,cores=1,threads=1,sockets=1,maxcpus=1 \
        -kernel /root/split-kernel/vmlinux.pvm \
        -append "console=ttyS0 nopcid rdinit=/bin/init subcmd=${subcmd} ctrip=${IP}" \
        -bios /opt/kata/share/kata-qemu/qemu/qboot.rom \
        -initrd ../initramfs/${app}.cpio.gz \
        -netdev tap,id=net0,ifname=tap-pk1,script=no,downscript=no,vhost=on -device virtio-net-pci,netdev=net0