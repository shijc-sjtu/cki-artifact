#!/bin/sh

sudo umount /mnt
sudo qemu-nbd -d /dev/nbd2