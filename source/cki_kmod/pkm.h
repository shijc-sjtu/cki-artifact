#ifndef PKM_H
#define PKM_H

#include <linux/ioctl.h>

#define PKM     0xBF

#define PKM_CREATE_CNTR          _IOW(PKM, 0x01, struct pkm_cntr_args)

// #define PKM_CNTR_LOAD_PKERNEL   1
#define PKM_CNTR_START           _IOW(PKM, 0x11, struct pkm_cntr_start_args)
#define PKM_CNTR_GET_PHYS_BASE   _IOW(PKM, 0x12, unsigned long)
#define PKM_CNTR_GET_PHYS_BASE2  _IOW(PKM, 0x13, unsigned long)
#define PKM_CNTR_GET_VRING_INFO  _IOW(PKM, 0x14, struct pkm_cntr_vring_args)
#define PKM_CNTR_SET_VRING_EFDS  _IOW(PKM, 0x15, struct pkm_cntr_vring_efds)

#define PKM_CNTR_MEM_SIZE       (1UL << 30)
#define PKM_CNTR_INITRD_SIZE    (1UL << 30)

#define PKM_CNTR_EXIT_CPU_UP            101
#define PKM_CNTR_EXIT_VRING_READY       201
#define PKM_CNTR_EXIT_PRINT             301

struct pkm_cntr_args {
        unsigned long mem;
        unsigned long mem2;
        unsigned long initrd;
        unsigned long initrd_size;
        char ip[32];
        char subcmd[32];
};

struct pkm_cntr_start_args {
        unsigned int cpu;
        unsigned long arg1;
        unsigned long arg2;
};

struct pkm_cntr_vring_args {
        unsigned int idx;
        unsigned short size;
        unsigned long desc_table;
        unsigned long avail_ring;
        unsigned long used_ring;
};

struct pkm_cntr_vring_efds {
        unsigned int idx;
        unsigned int kick;
        unsigned int call;
};

struct pkm_pkernel_segment {
        unsigned long base;
        unsigned long size;
        unsigned long paddr;
};

#endif /* PKM_H */