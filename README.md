# CKI Artifact

This repository contains the artifact of the EuroSys '25 paper "A Hardware-Software Co-Design for Efficient Secure Containers". Author: Jiacheng Shi, Yang Yu, Jinyu Gu, and Yubin Xia (Shanghai Jiao Tong University).

The artifact includes the source code of our prototype system (`source`), the scripts for running experiments in the paper (`scripts`), and the applications used for the experiments (`apps`).
Additionally, the artifact also includes a prebuilt VM image for running the experiments with a nested virtualization setting (containers running inside the VM).

## Hardware dependencies

This artifact requires AMD CPUs and has been tested on AMD EPYC-9654 and AMD EPYC-7T83 machines.
The machine should have at least 8 GB of memory and 32 GB of free disk space.

## Software dependencies

To run the experiments with the prebuilt images, the machine should operate on a Linux OS (Ubuntu 22.04 with Linux 6.7.0-rc6 tested) with the following dependencies installed:
qemu-system-x86_64 (v6.2.0 tested), expect, redis-cli, nc, docker, Python 3, matplotlib, and numpy.

## Benchmarks

The Dockerfiles for building container applications are available in the `apps` directory:
`parsec` (canneal, dedup, fluidanimate, freqmine), `vmitosis` (btree, xsbench), `memcached`, `redis` and `sqlite`.
Memcached and Redis are evaluated using memtier\_benchmark, which can be fetched from Docker Hub.

## Set-up

Steps 1-4 are required for all experiments:

1. Download the prebuilt images (`ubuntu-22.04.qcow2`, `bzImage-cki` and `bzImage-pvm`) from [link](https://www.dropbox.com/scl/fo/iedumaw4y9fg7tlx0ogde/ANYVteAdU3NT6ND-5PnTSC8?rlkey=x64edtal07735983yl6ah4zjt&st=6d0ktjv4&dl=0) and save the images in the `prebulit` directory.

2. Download and build QEMU v6.2.0.

```Bash
git clone -b v6.2.0 --depth=1 https://github.com/qemu/qemu.git
mkdir qemu/build && cd qemu/build
../configure --target-list=x86_64-softmmu
make -j$(nproc)
```

3. Configure the path of `qemu-system-x86_64` in `config.json` (`qemu` field).

4. Configure the password in `config.json` (`password` field), as some evaluation steps require superuser permissions (starting VM and running network configuration commands).

Steps 5-7 are only required for experiment *E3*:

5. Create a tap device on the host kernel with the following instruction, with `eth0` replaced with the name of your physical network card.

```Bash
sudo ./scripts/init_tap.sh eth0
```

6. Configure a static IP for the secure container in your local network in `config.json` (`container_ip` field).

7. Download memtier_benchmark from Docker Hub.

```Bash
docker pull redislabs/memtier_benchmark:latest
```

## Minimal Example

Run the PARSEC fluidanimate application with CKI using the following command.

```Bash
./scripts/run_simple.sh
```

The expected output is as follows.

```
PARSEC Benchmark Suite Version 3.0-beta-20150206
Loading file "/var/parsec/fluidanimate/in_300K.fluid"...
Number of cells: 135424
Grids steps over x, y, z: 0.00282609 0.0028125 0.00282609
Number of particles: 305809
Saving file "/tmp/out.fluid"...
```

## Major Claims 

Claim *C1*: Compared with HVM-NST and PVM, CKI reduces the latencies of page-fault-intensive applications (from PARSEC/vmitosis) by up to 70% and 44%, respectively. This is proven by experiment *E1* whose results are reported in Figure 12.

Claim *C2*: Compared with PVM, CKI increases the throughput of sqlite benchmark by up to 24%. This is proven by experiment *E2* whose results are reported in Figure 14.

Claim *C3*: Compared with HVM-NST, CKI-NST obtains 6.8x throughput for memcached and 2.0x throughput for Redis. This is proven by experiment *E3* whose results are reported in Figure 16.

## Experiments

Experiment *E1* [20 compute-minutes]: Run script `scripts/run_fig12.sh`, which executes the page-fault-intensive applications using HVM-NST, PVM, and CKI. The latency results will be displayed in `plots/fig12.pdf`.

Experiment *E2* [20 compute-minutes]: Run script `scripts/run_fig14.sh`, which executes the sqlite benchmark using HVM, PVM, and CKI. The throughput results will be displayed in `plots/fig14.pdf`.

Experiment *E3* [40 compute-minutes]: Run script `scripts/run_fig16.sh`, which evaluates the throughput of Redis and memcached using HVM, PVM, and CKI with different number of clients. The results will be displayed in `plots/fig16.pdf`.

Before running these scripts, the `log` and `plots` directories contain the script output and figures generated on our local machine.

## Source Code

`source/cki_kernel`: CKI guest/host kernel and KSM.

* The guest and host kernels in CKI share the same image.
The kernel is built from Linux v6.7-rc6, with the modifications specified in `source/cki_kernel/kernel.patch`.
The kernel configuration file is located at `source/cki_kernel/kernel.config`.

* For the guest kernel, the code for hooking privileged operations can be found in `arch/x86/pkernel/pkernel.c`.
Page table manipulation operations are redirected to the KSM (`arch/x86/pkernel/pkernel-monitor.c`) for security checks.

`source/cki_kmod`: CKI host kernel module.

* The host kernel code for launching secure containers and handling hypercalls like MMIO.

`launcher`: Host userspace code for launching secure containers.

`kata_config`: The configuration files for PVM and HVM containers.

The guest/host kernel code for PVM/HVM baselines is downloaded from [link](https://github.com/virt-pvm/linux). The VM image is built from [link](https://github.com/virt-pvm/misc/releases/download/test/pvm-kata-vm-img.tar.gz).
