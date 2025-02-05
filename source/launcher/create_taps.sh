#!/bin/bash -e

COUNT=${1:-1}

BR=br-pk
TAP=tap-pk
ETH=enp0s3

if [[ -z $(ip addr | grep $BR) ]]; then
    brctl addbr $BR
    ip addr flush dev $ETH
    brctl addif $BR $ETH
    ip link set $BR up
    dhclient $BR
fi

for i in $(seq $COUNT); do
    if [[ -z $(ip addr | grep $TAP$i) ]]; then
        ip tuntap add dev $TAP$i mode tap
        ip link set $TAP$i up
        brctl addif $BR $TAP$i
    fi
done
