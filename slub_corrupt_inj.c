
// SPDX-License-Identifier: GPL-2.0
/*
 * slub_corrupt_inj.c.: Slab Corruption Injector
 *
 * Written by Soumendu Sekhar Satapathy, 9th Feb 2020
 * satapathy.soumendu@gmail.com
 * This Kernel Module is intentionally made buggy to
 * inject SLUB corruption. This is written mainly for testing and reproducing
 * Slab corruption issues.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* related to file operations  */
#include <linux/errno.h>	/* error codes */
#include <linux/timer.h>
#include <linux/types.h>	/* size_t */
#include <linux/fcntl.h>	
#include <linux/hdreg.h>	
#include <linux/kdev_t.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	
#include <linux/bio.h>
#include <linux/blk_types.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/backing-dev.h>

#include <linux/mm.h>
#include <linux/swap.h> /* struct reclaim_state */
#include <linux/module.h>
#include <linux/bit_spinlock.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/seq_file.h>
#include <linux/kasan.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/mempolicy.h>
#include <linux/ctype.h>
#include <linux/debugobjects.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/math64.h>
#include <linux/fault-inject.h>
#include <linux/stacktrace.h>
#include <linux/prefetch.h>
#include <linux/memcontrol.h>
#include <linux/random.h>
#include <linux/smp.h>

#include <trace/events/kmem.h>

#include <scsi/scsi_dbg.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_driver.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>

#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>

#define TEST_ALLOC_SIZE 192

static int panicking_thread(void *arg)
{
	void* srq;
	void* tm;
    	struct task_struct *t_dma;
	int err1 = 0;
	int alloc_arr[34] = {8,16,24,32,40,48,56,64,72,80,88,96,104,112,120,128,136,144,152,160,168,176,184,192,512,1024,2048,4096,8192,16384,32768,65536,131072,262144};
	int i = 0;

	// This will try to allocate memory from all the Linux Kernel SLAB.
	// If CONFIG_SLUB_DEBUG_ON is set, then the dmesg ourput will show the
	// panic stack trace when the Linux Kernel Memory allocator calls
	// slab_alloc_node() to allocate slab object from the corrupted slab.
	for(i = 0; i < 34; i++)
	{
		srq = kmalloc(alloc_arr[i], GFP_KERNEL|GFP_DMA);
		if (!srq) {
			printk(KERN_WARNING, srq, "%s: kmalloc Sg_request "
			    	"failure\n", __func__);
			return ERR_PTR(-ENOMEM);
		}
		kfree(srq);
	}

    return 0;
}

// This is analogous to a DMA operation, writing to the un-mapped sg buffers
static int fw_doing_dma(void *arg)
{
    int this_cpu;
    int err1 = 0;
    struct task_struct *t_dma;

    this_cpu = get_cpu();

    printk(KERN_INFO "fw_doing_dma cpu = %d\n",this_cpu);
    printk(KERN_INFO "I am thread: %s[PID = %d]\n", current->comm, current->pid);

    // Writing to the Freed memory. Intentionally injecting Bug here.
    if(arg)
    	memset((char*)arg, 'c', TEST_ALLOC_SIZE);

    put_cpu();

    // Asynchronous thread is being scheduled, analogous to a DMA operation code path thread
    t_dma = kthread_run(panicking_thread, NULL, "thread-2");
    if (IS_ERR(t_dma)) {
       	printk(KERN_INFO "ERROR: Cannot create thread t_dma\n");
       	err1 = PTR_ERR(t_dma);
       	t_dma = NULL;
       	return -1;
    }

    return 0;
}


static int __init slub_corrupt_inj_init(void)
{
        struct slub_corrupt_inj_dev *dev;
	int err = 0;
	void* srq = NULL;

	struct scatterlist *sg_scmd;
	int err1 = 0;
	struct task_struct *t_dma;
	int this_cpu;
	struct scsi_cmnd *scmd;


	// Allocating Memory Here..
	srq = kmalloc(TEST_ALLOC_SIZE, GFP_KERNEL|GFP_DMA);
	if (!srq) {
		printk(KERN_WARNING, srq, "%s: kmalloc Sg list "
			    "failure\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	// Freeing the allocated memory here..
        kfree(srq);

	// Asynchronous thread is being scheduled, analogous to a DMA operation code path thread
	t_dma = kthread_run(fw_doing_dma, (void*)srq, "thread-1");
        if (IS_ERR(t_dma)) {
        	printk(KERN_INFO "ERROR: Cannot create thread ts1\n");
        	err1 = PTR_ERR(t_dma);
        	t_dma = NULL;
        	goto out_free;
    	}

        printk("slub_corrupt_inj: module loaded\n");
        return 0;

out_free:
        printk("slub_corrupt_inj: module NOT loaded !!!\n");
        return -ENOMEM;
}

static void __exit slub_corrupt_inj_exit(void)
{
        printk("slub_corrupt_inj: module unloaded !!!\n");
}
	
module_init(slub_corrupt_inj_init);
module_exit(slub_corrupt_inj_exit);

MODULE_ALIAS("slub_corrupt_inj");
MODULE_DESCRIPTION("Buggy Kernel Module as a SLUB corruption injector");
MODULE_AUTHOR("Soumendu Sekhar Satapathy <satapathy.soumendu@gmail.com>");
MODULE_LICENSE("GPL");
