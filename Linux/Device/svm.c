/*
 * svm.c
 * This file is part of VAMOS
 *
 * Copyright (C) 2017 - Maxul
 *
 * VAMOS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * VAMOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VAMOS. If not, see <http://www.gnu.org/licenses/>.
 */


/*
    Original: Ecular Xu <ecular_xu@trendmicro.com.cn>
    Revised:  Maxul Lee <maxul@bupt.edu.cn>
    Last modified: 2017.7.27
*/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/mm.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>

#include "svm.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxul");
MODULE_DESCRIPTION("VAMOS");

static void *svm_ram = NULL;
static void *svm_vmem = NULL;

static void taint(void)
{
    svm_ram = phys_to_virt(0x0);
    memset(svm_ram, 0x0, SVM_CAPACITY);
    memcpy(svm_ram, SVM_MAGIC, sizeof(SVM_MAGIC));
    *(long long unsigned int *)(svm_ram + SVM_OFFSET) = virt_to_phys(svm_vmem);
}

static void sweep(void)
{
    memset(svm_ram, 0x0, SVM_CAPACITY);
    svm_ram = svm_vmem = NULL;
}

/***************************************************************************
A long time ago, in v2.4, VM_RESERVED kept swapout process off VMA, currently
it lost original meaning but still has some effects:

     | effect                 | alternative flags
    -+------------------------+---------------------------------------------
    1| account as reserved_vm | VM_IO
    2| skip in core dump      | VM_IO, VM_DONTDUMP
    3| do not merge or expand | VM_IO, VM_DONTEXPAND, VM_HUGETLB, VM_PFNMAP
    4| do not mlock           | VM_IO, VM_DONTEXPAND, VM_HUGETLB, VM_PFNMAP

Thus VM_RESERVED can be replaced with VM_IO or pair VM_DONTEXPAND | VM_DONTDUMP.
***************************************************************************/
static int svm_mmap(struct file*filp, struct vm_area_struct *vma)
{
    vma->vm_flags |= VM_IO | (VM_DONTEXPAND | VM_DONTDUMP);

    /* map svm_vmem into user-mode space */
    if (remap_pfn_range(vma, vma->vm_start,
                        virt_to_phys(svm_vmem) >> PAGE_SHIFT,
                        vma->vm_end - vma->vm_start,
                        vma->vm_page_prot))
    {
        return  -EAGAIN;
    }

    printk(KERN_INFO "VAMOS: svm_mmap\n");
    return 0;
}

static const struct file_operations svm_fops =
{
    .owner = THIS_MODULE,
    .mmap = svm_mmap,
};

static struct miscdevice svm_dev = {
	.minor = SVM_MINOR,
	.name = SVM_NAME,
	.fops = &svm_fops,
	.mode = 0666,
};

static int __init device_init(void)
{
    int result = 0;

    result = misc_register(&svm_dev);
    if (result < 0) {
        printk(KERN_ERR "VAMOS: misc device register failed\n");
        return result;
    }

    /* Allocate contiguous physical memory in kernel */
    svm_vmem = kmalloc(SVM_SIZE, GFP_KERNEL);
    if (!svm_vmem) {
        printk(KERN_ERR "VAMOS: alloc' kernel memory failed\n");
        goto out;
    }
    memset(svm_vmem, 'z', SVM_SIZE);

    taint();
    printk(KERN_NOTICE "VAMOS: alloc' kernel memory at 0x%llx\n",
        virt_to_phys(svm_vmem));
    return 0;

out:
    return result;
}

static void __exit device_exit(void)
{
    if (svm_vmem) {
        kfree(svm_vmem);
    }
    misc_deregister(&svm_dev);

    sweep();
    printk(KERN_NOTICE "VAMOS: free memory successfully!\n");
}

module_init(device_init);
module_exit(device_exit);
