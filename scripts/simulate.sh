#!/bin/bash -e

if [[ $1 == "pvm" ]]; then
    kernel=prebuilt/bzImage-pvm
else
    kernel=prebuilt/bzImage-cki
fi

if [[ $2 == "tap" ]]; then
    netdev="tap,id=net0,ifname=tap-pk,script=no,downscript=no,vhost=on"
else
    netdev="user,id=net0,hostfwd=tcp::10022-:22"
fi

qemu=${QEMU:-qemu-system-x86_64}

sudo $qemu -enable-kvm -nographic \
    -m 8G -smp 4 -cpu host,pmu=off,tsc-freq=1000000000 -machine type=q35 -kernel ${kernel} \
    -append "root=/dev/vda1 console=ttyS0 hugepagesz=1G hugepages=4 norandmaps nokaslr mce=off nmi_watchdog=0 nowatchdog nosoftlockup" \
    -drive file=prebuilt/ubuntu-22.04.qcow2,if=none,id=disk0,format=qcow2 -device virtio-blk-pci,drive=disk0 \
    -netdev $netdev -device virtio-net-pci,netdev=net0,mac=52:54:00:12:34:99
