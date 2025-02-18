#!/bin/bash -e

app_name=$1

if [[ $app_name == "" ]]; then
    echo "Usage: build_app.sh app_name"
    exit 1
fi

image_name=split_kernel_${app_name}
base_name=${image_name}_base
cntr_name=$(basename $app_name)

pushd apps/$app_name
docker build -t $base_name .
popd

pushd apps/common
docker build -t $image_name -f Dockerfile --build-arg BASE_NAME=$base_name .
popd

rm -rf .rootfs
mkdir .rootfs
docker rm -f $cntr_name &>/dev/null || true
docker create --name $cntr_name $image_name
docker export $cntr_name | tar -C .rootfs -xf -
docker rm -f $cntr_name

pushd .rootfs
find . -not -name "initramfs.cpio.gz" -not -wholename "*/dev*" -print0 | \
        cpio --null -ov --format=newc | \
        gzip -9 > initramfs.cpio.gz
popd

scripts/mount_qcow2.sh prebuilt/ubuntu-22.04.qcow2
sudo mkdir -p /mnt/root/split-kernel/initramfs/$(dirname $app_name)
sudo cp .rootfs/initramfs.cpio.gz /mnt/root/split-kernel/initramfs/$app_name.cpio.gz
scripts/umount_qcow2.sh

rm -rf .rootfs
