#!/bin/sh

# ip=$(ip addr | grep "global eth0" | awk '{ print $2 }')
app=$1
subcmd=$2
# id="${3:-1}"
# ip="192.168.12.$(expr 239 + $id)/24"
ip="${3:-192.168.12.240/24}"
# ip="1.1.1.$(expr 1 + $id)/24"
# tap="tap-pk${id}"
tap="tap-pk1"


./tender ../vmlinux ../vmlinux.seg ../initramfs/${app}.cpio.gz $ip $tap $subcmd
