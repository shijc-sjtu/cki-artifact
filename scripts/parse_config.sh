#!/bin/bash -e

qemu=$(jq -r ".qemu" config.json)
password=$(jq -r ".password" config.json)
container_ip=$(jq -r ".container_ip" config.json)

IFS="/"
read -ra parts <<< "$container_ip"
server_ip=${parts[0]}
IFS=""
