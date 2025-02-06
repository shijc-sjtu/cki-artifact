#define pr_fmt(fmt) "pkm: "fmt
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/acpi.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/rwlock.h>
#include <linux/sched.h>
#include <linux/rcuwait.h>
#include <linux/cpumask.h>
#include <linux/virtio_config.h>
#include <linux/virtio_net.h>
#include <linux/virtio_ring.h>
#include <linux/eventfd.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <net/netfilter/br_netfilter.h>
#include <net/udp.h>
#include <asm/tsc.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/bootparam.h>
#include <asm/pgtable_types.h>
#include <asm/setup.h>
#include <asm/special_insns.h>
#include <asm/e820/types.h>
#include <asm/desc.h>
#include <asm/cmpxchg.h>

#include <asm/pkernel.h>
#include "pkm.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("XXX");
MODULE_DESCRIPTION("XXX");
MODULE_VERSION("0.01");

#define PKM_MINOR       123

#define PKM_CNTR_MEM_NPAGES     (PKM_CNTR_MEM_SIZE / PAGE_SIZE)
#define PKM_CNTR_INITRD_NPAGES  (PKM_CNTR_INITRD_SIZE / PAGE_SIZE)

#define PKM_CNTR_MIN_PCID       10
#define PKM_CNTR_MAX_PCID       100

#define LINUX_BOOT_OFFSET       0x1000080UL

struct pkm_acpi_tables {
        struct acpi_table_rsdp *rsdp;
        struct acpi_table_xsdt *xsdt;
        struct acpi_table_madt *madt;
};

typedef void (*pk_rkret_fn_t)(void *ctx);

struct pkm_cntr_cpu {
        struct pk_ctx ctx;
        struct rcuwait wait;
        struct mutex lock;
        pk_events_t events;
        volatile int virtio_event_cnt;
        struct task_struct *task;
        pk_rkret_fn_t pk_rkret;
};

#define PKM_VIRTIO_NET_QUEUE_NUM        2
#define PKM_VIRTIO_NET_QUEUE_SIZE       256

struct pkm_virtio_queue {
        struct pkm_cntr *cntr;
        unsigned short max_size;
        unsigned short size;
        bool ready;
        unsigned long desc_table;
        unsigned long avail_ring;
        unsigned long used_ring;
        struct eventfd_ctx *kick;
        struct eventfd_ctx *call;
        wait_queue_entry_t wait;
        poll_table pt;
};

struct pkm_virtio_net {
        unsigned int device_status;
        unsigned int features_select;
        unsigned long avail_features;
        unsigned int acked_features_select;
        unsigned long acked_features;
        unsigned int queue_select;
        struct pkm_virtio_queue queues[PKM_VIRTIO_NET_QUEUE_NUM];
};

#define array_count(arr) (sizeof(arr) / sizeof(arr[0]))

#define virtio_queue_set_idx(virtio, idx, attr, val)            \
        if ((idx) < array_count((virtio)->queues)) {             \
                (virtio)->queues[(idx)].attr = (val);           \
        }          

#define virtio_queue_set(virtio, attr, val) \
        virtio_queue_set_idx(virtio, (virtio)->queue_select, attr, val)                                                                    \

#define virtio_queue_get_idx(virtio, idx, attr) \
        ((idx) < array_count((virtio)->queues) ? (virtio)->queues[(idx)].attr : 0)

#define virtio_queue_get(virtio, attr) \
        virtio_queue_get_idx(virtio, (virtio)->queue_select, attr)

struct pkm_cntr {
        unsigned long n_cpus;
        unsigned long pcid;
        struct page *mem_hpage;
        struct page *mem2_hpage;
        struct page *initrd_hpage;
        pgdval_t *boot_pgtbl;
        struct boot_params *boot_params;
        struct net_device *netdev;
        unsigned char br_addr[6];
        struct sk_buff_head rx_pkts;
        struct pkm_cntr_cpu *cpus;
        struct pkm_virtio_net virtio_net;
};

static unsigned int next_cntr_pcid = PKM_CNTR_MIN_PCID;

static unsigned long host_idt_base;

static pgdval_t *create_boot_pgtbl(void)
{
        void *current_pgtbl;
	unsigned long *boot_pgtbl, *boot_pud;
	unsigned long i;

	boot_pgtbl = (void *)__get_free_page(GFP_KERNEL);
        if (!boot_pgtbl) {
                goto out_fail;
        }

	boot_pud = (void *)__get_free_page(GFP_KERNEL);
        if (!boot_pud) {
                goto out_free_boot_pgtbl;
        }

        current_pgtbl = phys_to_virt(read_cr3_pa());
	memcpy(boot_pgtbl, current_pgtbl, PAGE_SIZE);
	
        for (i = 0; i < PAGE_SIZE / sizeof(pteval_t); i++) {
		boot_pud[i] = _PAGE_PRESENT | _PAGE_RW | _PAGE_PSE | _PAGE_GLOBAL | (i << 30);
	}
	
        boot_pgtbl[0] = _PAGE_PRESENT | _PAGE_RW | virt_to_phys(boot_pud);

        return boot_pgtbl;

out_free_boot_pgtbl:
        free_page((unsigned long)boot_pgtbl);
out_fail:
	return NULL;
}

static void free_boot_pgtbl(pgdval_t *boot_pgtbl)
{
        void *boot_pud;

        boot_pud = phys_to_virt(*boot_pgtbl & ~(_PAGE_PRESENT | _PAGE_RW));

        free_page((unsigned long)boot_pud);
        free_page((unsigned long)boot_pgtbl);
}

static struct boot_params *create_boot_params(struct pkm_cntr *cntr,
                                              struct pkm_cntr_args *args)
{
        struct boot_params *boot_params;
        struct pk_boot_params *pk_boot_params;
        char *cmdline;

        boot_params = (void *)__get_free_page(GFP_KERNEL);
        if (!boot_params) {
                goto out_fail;
        }

        pk_copy_boot_params(boot_params);

        cmdline = (void *)__get_free_page(GFP_KERNEL);
        if (!cmdline) {
                goto out_free_boot_params;
        }

        snprintf(cmdline, COMMAND_LINE_SIZE, "initrd=0x%llx,0x%lx rdinit=/bin/init ctrip=%s subcmd=%s", page_to_phys(cntr->initrd_hpage), args->initrd_size, args->ip, args->subcmd);
	strlcat(cmdline, " norandmaps nokaslr pci=off console=null acpi=off rodata=off mce=off nmi_watchdog=0 nowatchdog nosoftlockup nopcid virtio_mmio.device=1K@0x10000:48", COMMAND_LINE_SIZE);
        boot_params->hdr.cmd_line_ptr = (unsigned int)virt_to_phys(cmdline);
	boot_params->ext_cmd_line_ptr = (unsigned int)(virt_to_phys(cmdline) >> 32);

        boot_params->e820_entries = 3;
	boot_params->e820_table[0].addr = 0;
	boot_params->e820_table[0].size = 0x1000;
	boot_params->e820_table[0].type = E820_TYPE_RESERVED;
        boot_params->e820_table[1].addr = page_to_phys(cntr->mem_hpage);
	boot_params->e820_table[1].size = PKM_CNTR_MEM_SIZE;
	boot_params->e820_table[1].type = E820_TYPE_RAM;
        boot_params->e820_table[2].addr = page_to_phys(cntr->mem2_hpage);
	boot_params->e820_table[2].size = PKM_CNTR_MEM_SIZE;
	boot_params->e820_table[2].type = E820_TYPE_RAM;

        boot_params->screen_info.orig_video_isVGA = VIDEO_TYPE_EFI;

        pk_boot_params = pk_get_boot_params(boot_params);
        pk_boot_params->magic = PK_BOOT_MAGIC;
        pk_boot_params->pcid = cntr->pcid;
        pk_boot_params->rk_cr3 = __read_cr3();
        pk_boot_params->rk_ctx = &cntr->cpus[0].ctx;
        pk_boot_params->n_cpus = cntr->n_cpus;

        return boot_params;

out_free_boot_params:
        free_page((unsigned long)boot_params);
out_fail:
        return NULL;
}

static void free_boot_params(struct boot_params *boot_params)
{
        unsigned long cmdline_paddr;

        cmdline_paddr = ((unsigned long)boot_params->ext_cmd_line_ptr << 32) +
                        boot_params->hdr.cmd_line_ptr;
        
        free_page((unsigned long)phys_to_virt(cmdline_paddr));
        free_page((unsigned long)boot_params);
}

static void init_virtio_net(struct pkm_cntr *cntr)
{
        int i;
        
        cntr->virtio_net.device_status = 0;
        cntr->virtio_net.features_select = 0;
        cntr->virtio_net.avail_features = 
                (1ULL << VIRTIO_F_NOTIFY_ON_EMPTY) |
                (1ULL << VIRTIO_RING_F_INDIRECT_DESC) |
                (1ULL << VIRTIO_RING_F_EVENT_IDX) |
                (1ULL << VIRTIO_F_ANY_LAYOUT) |
                (1ULL << VIRTIO_F_VERSION_1) |
                (1ULL << VIRTIO_NET_F_MRG_RXBUF) |
                (1ULL << VIRTIO_F_ACCESS_PLATFORM);
        cntr->virtio_net.acked_features = 0;
        cntr->virtio_net.queue_select = 0;

        for (i = 0; i < PKM_VIRTIO_NET_QUEUE_NUM; i++) {
                cntr->virtio_net.queues[i].cntr = cntr;
                cntr->virtio_net.queues[i].ready = 0;
                cntr->virtio_net.queues[i].max_size = PKM_VIRTIO_NET_QUEUE_SIZE;
                cntr->virtio_net.queues[i].kick = NULL;
                cntr->virtio_net.queues[i].call = NULL;
        }
}

static void init_ctx(struct pkm_cntr *cntr, struct pkm_cntr_cpu *cpu, bool primary)
{
        cpu->ctx.pmode.cr4 = native_read_cr4();

        if (primary) {
                cpu->ctx.pmode.rsi = virt_to_phys(cntr->boot_params);
                cpu->ctx.pmode.rip = page_to_phys(cntr->mem_hpage) + LINUX_BOOT_OFFSET;
                cpu->ctx.pmode.cr3 = virt_to_phys(cntr->boot_pgtbl);
        } else {
                cpu->ctx.pmode.rsi = (unsigned long)&cpu->ctx;
                cpu->ctx.pmode.rip = pk_get_secondary_entry();
                cpu->ctx.pmode.cr3 = page_to_phys(cntr->mem_hpage) + pk_get_init_top_pgt();
                cpu->ctx.pmode.rsp = pk_get_boot_stack();
        }
}

static void init_ctxs(struct pkm_cntr *cntr)
{
        unsigned int i;

        for (i = 0; i < cntr->n_cpus; i++) {
                init_ctx(cntr, &cntr->cpus[i], i == 0);
        }
}

static struct pkm_cntr_cpu *create_cpus(struct pkm_cntr *cntr)
{
        unsigned int i;
        struct pkm_cntr_cpu *cpus;
        
        cpus = kcalloc(cntr->n_cpus,
                          sizeof(struct pkm_cntr_cpu),
                          GFP_KERNEL);
        if (!cpus) {
                goto out;
        }
        
        for (i = 0; i < cntr->n_cpus; i++) {
                rcuwait_init(&cpus[i].wait);
                mutex_init(&cpus[i].lock);
                // cpus[i].events = 0;
                cpus[i].virtio_event_cnt = 0;
                cpus[i].pk_rkret = pk_rkret_slow;
        }

out:
        return cpus;
}

static void free_cpus(struct pkm_cntr_cpu *cpus)
{
        kfree(cpus);
}

static void deinit_virtio_net(struct pkm_virtio_net *virtio)
{
        int i;
        unsigned long long cnt;

        for (i = 0; i < PKM_VIRTIO_NET_QUEUE_NUM; i++) {
                if (virtio->queues[i].kick) {
                        eventfd_ctx_put(virtio->queues[i].kick);
                }
                if (virtio->queues[i].call) {
                        eventfd_ctx_remove_wait_queue(
                                virtio->queues[i].call, &virtio->queues[i].wait, &cnt);
                        eventfd_ctx_put(virtio->queues[i].call);
                }
        }
}

static int pkm_cntr_release(struct inode *inode, struct file *filp)
{
        struct pkm_cntr *cntr = filp->private_data;

        put_page(cntr->mem_hpage);
        put_page(cntr->mem2_hpage);
        put_page(cntr->initrd_hpage);
        free_boot_pgtbl(cntr->boot_pgtbl);
        free_boot_params(cntr->boot_params);
        free_cpus(cntr->cpus);
        deinit_virtio_net(&cntr->virtio_net);
        kfree(cntr);

        return 0;
}

static bool mem_check(struct pkm_cntr *cntr, unsigned long paddr)
{
        unsigned long mem_paddr, mem2_paddr, initrd_paddr;
        
        mem_paddr = page_to_phys(cntr->mem_hpage);
        if (paddr >= mem_paddr && paddr < mem_paddr + PKM_CNTR_MEM_SIZE) {
                return false;
        }
        mem2_paddr = page_to_phys(cntr->mem2_hpage);
        if (paddr >= mem2_paddr && paddr < mem2_paddr + PKM_CNTR_MEM_SIZE) {
                return false;
        }
        initrd_paddr = page_to_phys(cntr->initrd_hpage);
        if (paddr >= initrd_paddr && paddr < initrd_paddr + PKM_CNTR_INITRD_SIZE) {
                return false;
        }

        return true;
}

static long handle_rkcall_init_late(struct pkm_cntr_cpu *cpu)
{
        if (memcmp(cpu->ctx.pmode.idtr, cpu->ctx.rmode.idtr, 16)) {
                printk("idtr mismatch\n");
                return -1;
        }
        if (cpu->ctx.pmode.cr4 != cpu->ctx.rmode.cr4) {
                printk("CR4 mismatch 0x%lx 0x%lx\n", cpu->ctx.pmode.cr4, cpu->ctx.rmode.cr4);
                return -1;
        }

        return 0;
}

static long handle_rkcall_halt(struct pkm_cntr_cpu *cpu)
{
        while (!cpu->virtio_event_cnt)
                yield();
        return 0;
}

static unsigned int handle_mmio_read_avail_features(struct pkm_cntr *cntr)
{
        switch (cntr->virtio_net.features_select) {
        case 0:
                return (unsigned int)cntr->virtio_net.avail_features;
        case 1:
                return (unsigned int)(cntr->virtio_net.avail_features >> 32);
        default:
                return 0;
        }
}

static long handle_rkcall_mmio_read(struct pkm_cntr *cntr, unsigned long offset, int size)
{
        if (offset == 0x0 && size == 4) {
                return 0x74726976;
        } else if (offset == 0x4 && size == 4) {
                return 0x2;
        } else if (offset == 0x8 && size == 4) {
                return 0x1;
        } else if (offset == 0xc && size == 4) {
                return 0x1af4;
        } else if (offset == 0x70 && size == 4) {
                return cntr->virtio_net.device_status;
        } else if (offset == 0x10 && size == 4) {
                return handle_mmio_read_avail_features(cntr);
        } else if (offset == 0x44 && size == 4) {
                return virtio_queue_get(&cntr->virtio_net, ready);
        } else if (offset == 0x34 && size == 4) {
                return virtio_queue_get(&cntr->virtio_net, max_size);
        } else {
                while (1);
        }

        return 0;
}

static void handle_mmio_write_device_status(struct pkm_cntr *cntr, unsigned int status)
{
        unsigned int old = cntr->virtio_net.device_status;
        unsigned int diff = ~old & status;

        if (status == 0) {
                /* nothing */
        } else if (diff == VIRTIO_CONFIG_S_ACKNOWLEDGE && old == 0) {
                cntr->virtio_net.device_status = status;
        } else if (diff == VIRTIO_CONFIG_S_DRIVER && old == VIRTIO_CONFIG_S_ACKNOWLEDGE) {
                cntr->virtio_net.device_status = status;
        } else if (diff == VIRTIO_CONFIG_S_FEATURES_OK && old == (VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER)) {
                cntr->virtio_net.device_status = status;
        } else if (diff == VIRTIO_CONFIG_S_DRIVER_OK && old == (VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER | VIRTIO_CONFIG_S_FEATURES_OK)) {
                cntr->virtio_net.device_status = status;
        } else {
                while (1);
        }
}

static void handle_mmio_write_acked_features(struct pkm_cntr *cntr, unsigned int features)
{
        unsigned long acked_features;

        if (!(cntr->virtio_net.device_status & VIRTIO_CONFIG_S_DRIVER))
                return;
        if (cntr->virtio_net.device_status & VIRTIO_CONFIG_S_FEATURES_OK)
                return;
        if (cntr->virtio_net.device_status & VIRTIO_CONFIG_S_FAILED)
                return;

        switch (cntr->virtio_net.acked_features_select) {
        case 0:
                acked_features = features;
                break;
        case 1:
                acked_features = (unsigned long)features << 32;
                break;
        default:
                acked_features = 0;
                break;
        }

        acked_features &= cntr->virtio_net.avail_features;
        cntr->virtio_net.acked_features |= acked_features;
}


static void handle_mmio_write_queue_notify(struct pkm_cntr *cntr, unsigned int idx)
{
        if (idx >= PKM_VIRTIO_NET_QUEUE_NUM) {
                return;
        }

        eventfd_signal(cntr->virtio_net.queues[idx].kick, 1);
}

static int handle_rkcall_mmio_write(struct pkm_cntr *cntr, unsigned long val, unsigned long offset, int size)
{
        int r = 0;
        unsigned long lo, hi;

        if (offset == 0x70 && size == 4) {
                handle_mmio_write_device_status(cntr, val);
        } else if (offset == 0x14 && size == 4) {
                cntr->virtio_net.features_select = val;
        } else if (offset == 0x24 && size == 4) {
                cntr->virtio_net.acked_features_select = val;
        } else if (offset == 0x20 && size == 4) {
                handle_mmio_write_acked_features(cntr, val);
        } else if (offset == 0x30 && size == 4) {
                cntr->virtio_net.queue_select = val;
        } else if (offset == 0x38 && size == 4) {
                virtio_queue_set(&cntr->virtio_net, size, val);
        } else if (offset == 0x80 && size == 4) {
                hi = virtio_queue_get(&cntr->virtio_net, desc_table) >> 32 << 32;
                virtio_queue_set(&cntr->virtio_net, desc_table, hi | val);
        } else if (offset == 0x84 && size == 4) {
                lo = virtio_queue_get(&cntr->virtio_net, desc_table) << 32 >> 32;
                virtio_queue_set(&cntr->virtio_net, desc_table, (val << 32) | lo);
        } else if (offset == 0x90 && size == 4) {
                hi = virtio_queue_get(&cntr->virtio_net, avail_ring) >> 32 << 32;
                virtio_queue_set(&cntr->virtio_net, avail_ring, hi | val);
        } else if (offset == 0x94 && size == 4) {
                lo = virtio_queue_get(&cntr->virtio_net, avail_ring) << 32 >> 32;
                virtio_queue_set(&cntr->virtio_net, avail_ring, (val << 32) | lo);
        } else if (offset == 0xa0 && size == 4) {
                hi = virtio_queue_get(&cntr->virtio_net, used_ring) >> 32 << 32;
                virtio_queue_set(&cntr->virtio_net, used_ring, hi | val);
        } else if (offset == 0xa4 && size == 4) {
                lo = virtio_queue_get(&cntr->virtio_net, used_ring) << 32 >> 32;
                virtio_queue_set(&cntr->virtio_net, used_ring, (val << 32) | lo);
        } else if (offset == 0x44 && size == 4) {
                virtio_queue_set(&cntr->virtio_net, ready, val);
                r = val;
        } else if (offset == 0x50 && size == 4) {
                handle_mmio_write_queue_notify(cntr, val);
        } else{
                while (1);
        }

        return r;
}

void pkm_do_host_nmi_irqoff(void);
void pkm_do_host_interrupt_irqoff(unsigned long entry);

static long pkm_cntr_ioctl_start(struct pkm_cntr *cntr, unsigned long arg)
{
        long r, ret;
        int count;
        unsigned long n, arg1, arg2, arg3;
        unsigned long flags;
        pk_events_t events;
        struct pkm_cntr_cpu *cpu;
        struct pkm_cntr_start_args kargs;

        if (copy_from_user(&kargs, (void *)arg, sizeof(kargs))) {
                r = -EFAULT;
                goto out_fail;
        }

        if (kargs.cpu >= cntr->n_cpus) {
                r = -EINVAL;
                goto out_fail;
        }

        cpu = &cntr->cpus[kargs.cpu];
        if (!mutex_trylock(&cpu->lock)) {
                r = -EAGAIN;
                goto out_fail;
        }

        cpu->task = current;

        r = set_cpus_allowed_ptr(current, cpumask_of(kargs.cpu));
        if (r) {
                goto out_unlock;
        }

        for (;;) {
                cpu->ctx.pmode.cr3 &= ~X86_CR3_PCID_MASK;
                cpu->ctx.pmode.cr3 |= cntr->pcid;

                local_irq_save(flags);

                cpu->pk_rkret(&cpu->ctx);

                n = cpu->ctx.pmode.rdx;
                arg1 = cpu->ctx.pmode.rcx;
                arg2 = cpu->ctx.pmode.r8;
                arg3 = cpu->ctx.pmode.r9;

                if (n == PK_RKCALL_INTR) {
                        gate_desc *desc = (gate_desc *)host_idt_base + arg1;
                        pkm_do_host_interrupt_irqoff(gate_offset(desc));
                }

                local_irq_restore(flags);

                switch (n) {
                case PK_RKCALL_DEBUG:
                        printk("debug(%u): %lu, 0x%lx, 0x%lx\n", kargs.cpu, arg1, arg2, arg3);
                        ret = 0;
                        break;
                case PK_RKCALL_MEM_CHECK:
                        ret = mem_check(cntr, arg1);
                        if (ret)
                                printk("mem_check: 0x%lx\n", arg1);
                        break;
                case PK_RKCALL_SHUTDOWN:
                        r = 0;
                        goto out_unlock;
                case PK_RKCALL_INIT_LATE:
                        ret = handle_rkcall_init_late(cpu);
                        if (!ret)
                                cpu->pk_rkret = pk_rkret_fast;
                        break;
                case PK_RKCALL_WRITE_CR3:
                        cpu->ctx.pmode.cr3 = arg1;
                        ret = 0;
                        break;
                case PK_RKCALL_NOP:
                        ret = 0;
                        break;
                case PK_RKCALL_INTR:
                        break;
                case PK_RKCALL_HALT:
                        ret = handle_rkcall_halt(cpu);
                        break;
                case PK_RKCALL_CPU_UP:
                        r = PKM_CNTR_EXIT_CPU_UP;
                        kargs.arg1 = arg1;
                        goto out_copy_args;
                case PK_RKCALL_YIELD:
                        yield();
                        break;
                case PK_RKCALL_MMIO_READ:
                        ret = handle_rkcall_mmio_read(cntr, arg1, arg2);
                        break;
                case PK_RKCALL_MMIO_WRITE:
                        ret = handle_rkcall_mmio_write(cntr, arg1, arg2, arg3);
                        if (ret) {
                                r = PKM_CNTR_EXIT_VRING_READY;
                                kargs.arg1 = cntr->virtio_net.queue_select;
                                goto out_copy_args;
                        }
                        break;
                case PK_RKCALL_PRINT:
                        r = PKM_CNTR_EXIT_PRINT;
                        kargs.arg1 = arg1;
                        kargs.arg2 = arg2;
                        goto out_copy_args;
                default:
                        r = -EINVAL;
                        goto out_unlock;
                }

                events = 0;
                count = cpu->virtio_event_cnt;
                if (count) {
                        arch_cmpxchg(&cpu->virtio_event_cnt, count, count - 1);
                        events = PK_EVENT_VIRTIO_INTR;
                }

                cpu->ctx.pmode.rsi = ret;
                cpu->ctx.pmode.rdx = events;
        }

out_copy_args:
        if (copy_to_user((void *)arg, &kargs, sizeof(kargs))) {
                r = -EFAULT;
        }
out_unlock:
        mutex_unlock(&cpu->lock);
out_fail:
        return r;
}

static long pkm_cntr_ioctl_get_phys_base(struct pkm_cntr *cntr, unsigned long arg)
{
        unsigned long r, phys_base;

        phys_base = page_to_phys(cntr->mem_hpage);

        r = copy_to_user((void *)arg, &phys_base, sizeof(phys_base));

        return r ? -EFAULT : 0;
}

static long pkm_cntr_ioctl_get_phys_base2(struct pkm_cntr *cntr, unsigned long arg)
{
        unsigned long r, phys_base;

        phys_base = page_to_phys(cntr->mem2_hpage);

        r = copy_to_user((void *)arg, &phys_base, sizeof(phys_base));

        return r ? -EFAULT : 0;
}

static long pkm_cntr_ioctl_get_vring_info(struct pkm_cntr *cntr, unsigned long arg)
{
        int r;
        struct pkm_cntr_vring_args kargs;

        r = copy_from_user(&kargs, (void *)arg, sizeof(kargs));
        if (r) {
                return -EFAULT;
        }

        kargs.size = virtio_queue_get_idx(&cntr->virtio_net, kargs.idx, size);
        kargs.avail_ring = virtio_queue_get_idx(&cntr->virtio_net, kargs.idx, avail_ring);
        kargs.desc_table = virtio_queue_get_idx(&cntr->virtio_net, kargs.idx, desc_table);
        kargs.used_ring = virtio_queue_get_idx(&cntr->virtio_net, kargs.idx, used_ring);

        // printk("vring: %llx, %lx %lx %lx\n", page_to_phys(cntr->mem_hpage), kargs.avail_ring, kargs.desc_table, kargs.used_ring);

        r = copy_to_user((void *)arg, &kargs, sizeof(kargs));
        if (r) {
                return -EFAULT;
        }

        return 0;
}

static int virtio_wakeup(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
{
        struct pkm_virtio_queue *queue =
                container_of(wait, struct pkm_virtio_queue, wait);
        struct pkm_cntr *cntr = queue->cntr;
        __poll_t flags = key_to_poll(key);

        if (flags & EPOLLIN) {
                // int ret;
                __sync_fetch_and_add(&cntr->cpus[0].virtio_event_cnt, 1);
                // ret = rcuwait_wake_up(&cntr->cpus[0].wait);
        }

        return 0;
}

static void virtio_ptable_queue_proc(struct file *file,
				     wait_queue_head_t *wqh, poll_table *pt)
{
        struct pkm_virtio_queue *queue = container_of(pt, struct pkm_virtio_queue, pt);
	add_wait_queue(wqh, &queue->wait);
}

static long pkm_cntr_ioctl_set_vring_efds(struct pkm_cntr *cntr, unsigned long arg)
{
        int r;
        struct eventfd_ctx *kick_eventfd, *call_eventfd;
        struct file *call_file;
        struct pkm_cntr_vring_efds kefds;

        r = copy_from_user(&kefds, (void *)arg, sizeof(kefds));
        if (r) {
                r = -EFAULT;
                goto out_fail;
        }

        if (kefds.idx >= PKM_VIRTIO_NET_QUEUE_NUM) {
                r = -EINVAL;
                goto out_fail;
        }

        kick_eventfd = eventfd_ctx_fdget(kefds.kick);
        if (IS_ERR(kick_eventfd)) {
                r = PTR_ERR(kick_eventfd);
                goto out_fail;
        }

        call_eventfd = eventfd_ctx_fdget(kefds.call);
        if (IS_ERR(call_eventfd)) {
                r = PTR_ERR(call_eventfd);
                goto out_put_kick;
        }

        call_file = eventfd_fget(kefds.call);
        if (IS_ERR(call_file)) {
                r = PTR_ERR(call_file);
                goto out_put_call;
        }

        init_waitqueue_func_entry(&cntr->virtio_net.queues[kefds.idx].wait, virtio_wakeup);
	init_poll_funcptr(&cntr->virtio_net.queues[kefds.idx].pt, virtio_ptable_queue_proc);

        vfs_poll(call_file, &cntr->virtio_net.queues[kefds.idx].pt);

        fput(call_file);

        cntr->virtio_net.queues[kefds.idx].kick = kick_eventfd;
        cntr->virtio_net.queues[kefds.idx].call = call_eventfd;

        return 0;

out_put_call:
        eventfd_ctx_put(call_eventfd);
out_put_kick:
        eventfd_ctx_put(kick_eventfd);
out_fail:
        return r;
}

static long pkm_cntr_ioctl(struct file *filp,
			   unsigned int ioctl, unsigned long arg)
{
        long r = -EINVAL;
        struct pkm_cntr *cntr = filp->private_data;

        switch (ioctl) {
        case PKM_CNTR_START:
                r = pkm_cntr_ioctl_start(cntr, arg);
                break;
        case PKM_CNTR_GET_PHYS_BASE:
                r = pkm_cntr_ioctl_get_phys_base(cntr, arg);
                break;
        case PKM_CNTR_GET_PHYS_BASE2:
                r = pkm_cntr_ioctl_get_phys_base2(cntr, arg);
                break;
        case PKM_CNTR_GET_VRING_INFO:
                r = pkm_cntr_ioctl_get_vring_info(cntr, arg);
                break;
        case PKM_CNTR_SET_VRING_EFDS:
                r = pkm_cntr_ioctl_set_vring_efds(cntr, arg);
                break;
        default:
                break;
        }

        return r;
}

static struct file_operations pkm_cntr_fops = {
	.release        = pkm_cntr_release,
	.unlocked_ioctl = pkm_cntr_ioctl,
	.llseek		= noop_llseek,
};

static long pkm_dev_ioctl_create_cntr(unsigned long arg)
{
        int r, fd;
        struct pkm_cntr *cntr;
        struct file *file;
        struct pkm_cntr_args args;

        r = copy_from_user(&args, (void *)arg, sizeof(args));
        if (r) {
                goto out_fail;
        }

        cntr = kmalloc(sizeof(*cntr), GFP_KERNEL);
        if (!cntr) {
                r = -ENOMEM;
                goto out_fail;
        }

        cntr->n_cpus = num_online_cpus();

        r = get_user_pages_fast(args.mem, 1, FOLL_WRITE, &cntr->mem_hpage);
        if (r != 1) {
                goto out_free_cntr;
        }
        if (compound_nr(cntr->mem_hpage) != PKM_CNTR_MEM_NPAGES) {
                r = -EINVAL;
                goto out_put_mem_hpage;
        }

        r = get_user_pages_fast(args.mem2, 1, FOLL_WRITE, &cntr->mem2_hpage);
        if (r != 1) {
                goto out_put_mem_hpage;
        }
        if (compound_nr(cntr->mem2_hpage) != PKM_CNTR_MEM_NPAGES) {
                r = -EINVAL;
                goto out_put_mem2_hpage;
        }

        r = get_user_pages_fast(args.initrd, 1, FOLL_WRITE, &cntr->initrd_hpage);
        if (r != 1) {
                goto out_put_mem2_hpage;
        }
        if (compound_nr(cntr->initrd_hpage) != PKM_CNTR_INITRD_NPAGES) {
                r = -EINVAL;
                goto out_put_initrd_hpage;
        }

        // magic = (void *)phys_to_virt(page_to_phys(cntr->mem_hpage)) + LINUX_BOOT_OFFSET;
        // printk("magic=0x%lx\n", *magic);
        // return -EINVAL;

        cntr->boot_pgtbl = create_boot_pgtbl();
        if (!cntr->boot_pgtbl) {
                r = -ENOMEM;
                goto out_put_initrd_hpage;
        }

        cntr->pcid = __sync_fetch_and_add(&next_cntr_pcid, 1);
        if (cntr->pcid > PKM_CNTR_MAX_PCID) {
                r = -EOVERFLOW;
                goto out_free_boot_pgtbl;
        }

        cntr->cpus = create_cpus(cntr);
        if (!cntr->cpus) {
                r = -ENOMEM;
                goto out_free_boot_pgtbl;
        }

        cntr->boot_params = create_boot_params(cntr, &args);
        if (!cntr->boot_params) {
                r = -ENOMEM;
                goto out_free_cpus;
        }

        init_virtio_net(cntr);

        init_ctxs(cntr);

        // r = init_acpi_tables(&cntr->acpi_tables);
        // if (r) {
        //         goto out_free_boot_params;
        // }

        skb_queue_head_init(&cntr->rx_pkts);

        fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
                r = fd;
                // goto out_deinit_acpi_tables;
                goto out_free_boot_params;
        }

        file = anon_inode_getfile("pkm-cntr", &pkm_cntr_fops, cntr, O_RDWR);
        if (IS_ERR(file)) {
                r = PTR_ERR(file);
                goto out_put_fd;
        }

        fd_install(fd, file);
        return fd;

out_put_fd:
        put_unused_fd(fd);
out_free_cpus:
        free_cpus(cntr->cpus);
out_free_boot_params:
        free_boot_params(cntr->boot_params);
out_free_boot_pgtbl:
        free_boot_pgtbl(cntr->boot_pgtbl);
out_put_initrd_hpage:
        put_page(cntr->initrd_hpage);
out_put_mem2_hpage:
        put_page(cntr->mem2_hpage);
out_put_mem_hpage:
        put_page(cntr->mem_hpage);
out_free_cntr:
        kfree(cntr);
out_fail:
        return r;
}

static long pkm_dev_ioctl(struct file *filp,
			  unsigned int ioctl, unsigned long arg)
{
        long r = -EINVAL;

        switch (ioctl) {
        case PKM_CREATE_CNTR:
                r = pkm_dev_ioctl_create_cntr(arg);
                break;
        default:
                break;
        }

        return r;
}

static struct file_operations pkm_chardev_ops = {
	.unlocked_ioctl = pkm_dev_ioctl,
	.llseek		= noop_llseek,
};

static struct miscdevice pkm_dev = {
	MISC_DYNAMIC_MINOR,
	"pkm",
	&pkm_chardev_ops,
};

static int __init pkm_init(void)
{
        struct desc_ptr dt;

	store_idt(&dt);
	host_idt_base = dt.address;

        return misc_register(&pkm_dev);
}

static void __exit pkm_exit(void)
{
        misc_deregister(&pkm_dev);
}

module_init(pkm_init);
module_exit(pkm_exit);
