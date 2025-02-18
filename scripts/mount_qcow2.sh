#!/bin/sh

sudo modprobe nbd max_part=63
sudo qemu-nbd -c /dev/nbd2 $1
sleep 1
sudo mount /dev/nbd2p1 /mnt
