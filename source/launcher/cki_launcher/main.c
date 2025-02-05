#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/vhost.h>
#include <linux/if_tun.h>
#include <linux/virtio_net.h>

#include "pkm.h"

#define MAP_HUGE_1GB    (30 << 26)

#define fatal(check, info) \
        if (check) { \
                perror(info); \
                exit(EXIT_FAILURE); \
        }

static int dev_fd, cntr_fd, vhost_net_fd, tap_fd;
static void *virt_base, *virt_base2;
static unsigned long phys_base, phys_base2;

#define phys2virt1(phys) ((phys) - phys_base + (unsigned long)virt_base)
#define phys2virt2(phys) ((phys) - phys_base2 + (unsigned long)virt_base2)
#define phys2virt(phys) ((phys) >= phys_base && (phys) < phys_base + PKM_CNTR_MEM_SIZE ? phys2virt1(phys) : phys2virt2(phys))

static void *percpu_routine(void *arg);

static void handle_exit_cpu_up(unsigned int cpu)
{
        int r;
        pthread_t pthread;

        r = pthread_create(&pthread, NULL, percpu_routine, (void *)(long)cpu);
        fatal(r < 0, "create percpu thread");
}

static void handle_exit_vring_ready(unsigned int idx)
{
        int r;
        int kick, call;
        struct pkm_cntr_vring_args args;
        struct pkm_cntr_vring_efds efds;
        struct vhost_vring_state state;
        struct vhost_vring_addr addr;
        struct vhost_vring_file file;

        args.idx = idx;
        r = ioctl(cntr_fd, PKM_CNTR_GET_VRING_INFO, &args);
        fatal(r < 0, "get vring info");

        state.index = idx;
        state.num = args.size;
        r = ioctl(vhost_net_fd, VHOST_SET_VRING_NUM, &state);
        fatal(r < 0, "vhost set-vring-num");

        state.index = idx;
        state.num = 0;
        r = ioctl(vhost_net_fd, VHOST_SET_VRING_BASE, &state);
        fatal(r < 0, "vhost set-vring-base");

        addr.index = idx;
        addr.flags = 0;
        addr.avail_user_addr = phys2virt(args.avail_ring);
        addr.desc_user_addr = phys2virt(args.desc_table);
        addr.used_user_addr = phys2virt(args.used_ring);
        addr.log_guest_addr = 0;
        r = ioctl(vhost_net_fd, VHOST_SET_VRING_ADDR, &addr);
        fatal(r < 0, "vhost set-vring-addr");

        file.index = idx;
        file.fd = tap_fd;
        r = ioctl(vhost_net_fd, VHOST_NET_SET_BACKEND, &file);
        fatal(r < 0, "vhost-net set-backend");

        kick = eventfd(0, EFD_CLOEXEC);
        fatal(kick < 0, "create kick eventfd");

        call = eventfd(0, EFD_CLOEXEC);
        fatal(call < 0, "create call eventfd");

        efds.idx = idx;
        efds.kick = kick;
        efds.call = call;
        r = ioctl(cntr_fd, PKM_CNTR_SET_VRING_EFDS, &efds);
        fatal(r < 0, "set vring efds");

        file.index = idx;
        file.fd = kick;
        r = ioctl(vhost_net_fd, VHOST_SET_VRING_KICK, &file);
        fatal(r < 0, "vhost-net set-vring-kick");

        file.index = idx;
        file.fd = call;
        r = ioctl(vhost_net_fd, VHOST_SET_VRING_CALL, &file);
        fatal(r < 0, "vhost-net set-vring-call");
}

static void handle_exit_print(unsigned long buf_phys, int size)
{
        char *buf = (char *)phys2virt(buf_phys);

        printf("%.*s", size, buf);
}

static void *percpu_routine(void *arg)
{
        int r;
        struct pkm_cntr_start_args args;
        
        args.cpu = (unsigned int)(long)arg;

        for (;;) {
                r = ioctl(cntr_fd, PKM_CNTR_START, &args);
                if (r == -EAGAIN) {
                        continue;
                } else if (r == PKM_CNTR_EXIT_CPU_UP) {
                        handle_exit_cpu_up(args.arg1);
                } else if (r == PKM_CNTR_EXIT_VRING_READY) {
                        handle_exit_vring_ready(args.arg1);
                } else if (r == PKM_CNTR_EXIT_PRINT) {
                        handle_exit_print(args.arg1, args.arg2);
                } else {
                        break;
                }
        }
        fatal(r < 0, "start cntr");

        return NULL;
}

static void *allocate_memory(unsigned long size)
{
        void *mem;
        
        mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
        fatal(mem == MAP_FAILED, "allocate memory");

        return mem;
}

static void load_kernel_image(const char *image_filename, const char *seg_filename)
{
        int r, img_fd;
        FILE *seg_file;
        struct pkm_pkernel_segment seg;

        img_fd = open(image_filename, O_RDONLY);
        fatal(img_fd < 0, "open vmlinux");

        seg_file = fopen(seg_filename, "r");
        fatal(!seg_file, "open vmlinux.seg");

        for (;;) {
                r = fscanf(seg_file, "0x%lx 0x%lx 0x%lx\n",
                           &seg.base, &seg.size, &seg.paddr);
                if (r == EOF) {
                        break;
                }

                r = pread(img_fd, virt_base + seg.paddr, seg.size, seg.base);
                fatal(r != seg.size, "read segment");
        }

        close(img_fd);
        fclose(seg_file);
}

static unsigned long load_initrd_image(void *initrd, const char *initrd_filename)
{
        int r, initrd_fd;

        initrd_fd = open(initrd_filename, O_RDONLY);
        fatal(initrd_fd < 0, "open initrd");

        r = read(initrd_fd, initrd, PKM_CNTR_INITRD_SIZE);
        fatal(r < 0, "read initrd");
        
        close(initrd_fd);

        return r;
}

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)
static int tap_open(char *ifname, int ifname_size, int *vnet_hdr,
                    int vnet_hdr_required, int mq_required)
{
    struct ifreq ifr;
    int fd, ret;
    int len = sizeof(struct virtio_net_hdr);
    unsigned int features;

    TFR(fd = open("/dev/net/tun", O_RDWR));
    if (fd < 0) {
        // error_setg_errno(errp, errno, "could not open %s", PATH_NET_TUN);
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

    if (ioctl(fd, TUNGETFEATURES, &features) == -1) {
        // warn_report("TUNGETFEATURES failed: %s", strerror(errno));
        features = 0;
    }

    if (features & IFF_ONE_QUEUE) {
        ifr.ifr_flags |= IFF_ONE_QUEUE;
    }

    if (*vnet_hdr) {
        if (features & IFF_VNET_HDR) {
            *vnet_hdr = 1;
            ifr.ifr_flags |= IFF_VNET_HDR;
        } else {
            *vnet_hdr = 0;
        }

        if (vnet_hdr_required && !*vnet_hdr) {
        //     error_setg(errp, "vnet_hdr=1 requested, but no kernel "
        //                "support for IFF_VNET_HDR available");
            close(fd);
            return -1;
        }
        /*
         * Make sure vnet header size has the default value: for a persistent
         * tap it might have been modified e.g. by another instance of qemu.
         * Ignore errors since old kernels do not support this ioctl: in this
         * case the header size implicitly has the correct value.
         */
        ioctl(fd, TUNSETVNETHDRSZ, &len);
    }

    if (mq_required) {
        if (!(features & IFF_MULTI_QUEUE)) {
        //     error_setg(errp, "multiqueue required, but no kernel "
        //                "support for IFF_MULTI_QUEUE available");
            close(fd);
            return -1;
        } else {
            ifr.ifr_flags |= IFF_MULTI_QUEUE;
        }
    }

    if (ifname[0] != '\0')
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    else
        strncpy(ifr.ifr_name, "tap%d", IFNAMSIZ);
    ret = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (ret != 0) {
        // if (ifname[0] != '\0') {
        //     error_setg_errno(errp, errno, "could not configure %s (%s)",
        //                      PATH_NET_TUN, ifr.ifr_name);
        // } else {
        //     error_setg_errno(errp, errno, "could not configure %s",
        //                      PATH_NET_TUN);
        // }
        close(fd);
        return -1;
    }
    strncpy(ifname, ifr.ifr_name, ifname_size);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static void init_vhost_net(const char *tap_name)
{
        int r, len;
        unsigned long features;
        // struct ifreq ifr;
        int vnet_hdr;
        char ifname[128];
        struct vhost_memory *vhost_memory;

        // tap_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
        // fatal(tap_fd < 0, "create raw socket");

        // strncpy(ifr.ifr_name, "tap0", IFNAMSIZ);
        // r = ioctl(tap_fd, SIOCGIFINDEX, &ifr);
        // fatal(r < 0, "SIOCGIFINDEX");

        vnet_hdr = 1;
        strncpy(ifname, tap_name, sizeof(ifname));
        tap_fd = tap_open(ifname, sizeof(ifname), &vnet_hdr, 0, 0);
        fatal(tap_fd < 0, "open tap");

        r = ioctl(tap_fd, TUNSETOFFLOAD, 0x1f);
        fatal(r < 0, "TUNSETOFFLOAD");

        len = 12;
        r = ioctl(tap_fd, TUNSETVNETHDRSZ, &len);
        fatal(r < 0, "TUNSETVNETHDRSZ");
        
        vhost_net_fd = open("/dev/vhost-net", O_RDWR);
        fatal(vhost_net_fd < 0, "open vhost-net");

        r = ioctl(vhost_net_fd, VHOST_SET_OWNER, 0);
        fatal(r < 0, "vhost set-owner");

        features = 0x130008000;
        r = ioctl(vhost_net_fd, VHOST_SET_FEATURES, &features);
        fatal(r < 0, "vhost set-features");

        vhost_memory = malloc(sizeof(*vhost_memory) + sizeof(*vhost_memory->regions));
        fatal(!vhost_memory, "alloc vhost-memory");
        memset(vhost_memory, 0, sizeof(*vhost_memory));

        vhost_memory->nregions = 1;
        vhost_memory->regions[0].guest_phys_addr = phys_base;
        vhost_memory->regions[0].memory_size = PKM_CNTR_MEM_SIZE;
        vhost_memory->regions[0].userspace_addr = (unsigned long)virt_base;

        r = ioctl(vhost_net_fd, VHOST_SET_MEM_TABLE, vhost_memory);
        fatal(r < 0, "vhost set-mem-table");

        free(vhost_memory);
}

static void create_cntr(void *initrd, unsigned long initrd_size, const char *ip, const char *subcmd)
{
        int r, dev_fd;
        struct pkm_cntr_args args;
        
        dev_fd = open("/dev/pkm", O_RDWR);
        fatal(dev_fd < 0, "open /dev/pkm");

        args.mem = (unsigned long)virt_base;
        args.mem2 = (unsigned long)virt_base2;
        args.initrd = (unsigned long)initrd;
        args.initrd_size = initrd_size;
        strcpy(args.ip, ip);
        strcpy(args.subcmd, subcmd);
        cntr_fd = ioctl(dev_fd, PKM_CREATE_CNTR, &args);
        fatal(cntr_fd < 0, "create cntr");

        r = ioctl(cntr_fd, PKM_CNTR_GET_PHYS_BASE, &phys_base);
        fatal(r < 0, "get phys base");

        r = ioctl(cntr_fd, PKM_CNTR_GET_PHYS_BASE2, &phys_base2);
        fatal(r < 0, "get phys base");
}

// static void cleanup(void)
// {
//         close(tap_fd);
//         close(vhost_net_fd);
//         close(cntr_fd);
//         close(dev_fd);
//         printf("succeed\n");
// }

int main(int argc, char *argv[])
{
        void *initrd;
        unsigned long initrd_size;
        
        virt_base = allocate_memory(PKM_CNTR_MEM_SIZE);
        load_kernel_image(argv[1], argv[2]);

        virt_base2 = allocate_memory(PKM_CNTR_MEM_SIZE);

        initrd = allocate_memory(PKM_CNTR_INITRD_SIZE);
        initrd_size = load_initrd_image(initrd, argv[3]);

        create_cntr(initrd, initrd_size, argv[4], argv[6]);
        init_vhost_net(argv[5]);
        
        percpu_routine((void *)0L);
        
        // cleanup();
        return 0;
}
