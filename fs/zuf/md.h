/*
 * Multi-Device operations.
 *
 * Copyright (c) 2018 NetApp Inc. All rights reserved.
 *
 * ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
 *
 * Authors:
 *	Boaz Harrosh <boazh@netapp.com>
 *	Sagi Manole <sagim@netapp.com>"
 */

#ifndef __MD_H__
#define __MD_H__

#include "zus_api.h"

struct md_t1_info {
	ulong phys_pfn;
	void *virt_addr;
	struct dax_device *dax_dev;
	struct dev_pagemap *pgmap;
};

struct md_t2_info {
	bool err_read_reported;
	bool err_write_reported;
};

struct md_dev_info {
	struct block_device *bdev;
	ulong size;
	ulong offset;
	union {
		struct md_t1_info	t1i;
		struct md_t2_info	t2i;
	};
	int index;
	int nid;
};

struct md_dev_larray {
	ulong bn_gcd;
	struct md_dev_info **map;
};

struct multi_devices {
	int dev_index;
	int t1_count;
	int t2_count;
	struct md_dev_info devs[MD_DEV_MAX];
	struct md_dev_larray t1a;
	struct md_dev_larray t2a;
};

static inline u64 md_p2o(ulong bn)
{
	return (u64)bn << PAGE_SHIFT;
}

static inline ulong md_o2p(u64 offset)
{
	return offset >> PAGE_SHIFT;
}

static inline ulong md_o2p_up(u64 offset)
{
	return md_o2p(offset + PAGE_SIZE - 1);
}

static inline struct md_dev_info *md_t1_dev(struct multi_devices *md, int i)
{
	return &md->devs[i];
}

static inline struct md_dev_info *md_t2_dev(struct multi_devices *md, int i)
{
	return &md->devs[md->t1_count + i];
}

static inline struct md_dev_info *md_dev_info(struct multi_devices *md, int i)
{
	return &md->devs[i];
}

static inline void *md_t1_addr(struct multi_devices *md, int i)
{
	struct md_dev_info *mdi = md_t1_dev(md, i);

	return mdi->t1i.virt_addr;
}

static inline struct md_dev_info *md_bn_t1_dev(struct multi_devices *md,
						 ulong bn)
{
	return md->t1a.map[bn / md->t1a.bn_gcd];
}

static inline ulong md_pfn(struct multi_devices *md, ulong block)
{
	struct md_dev_info *mdi = md_bn_t1_dev(md, block);

	return mdi->t1i.phys_pfn + (block - md_o2p(mdi->offset));
}

static inline void *md_addr(struct multi_devices *md, ulong offset)
{
	struct md_dev_info *mdi = md_bn_t1_dev(md, md_o2p(offset));

	return offset ? mdi->t1i.virt_addr + (offset - mdi->offset) : NULL;
}

static inline void *md_baddr(struct multi_devices *md, ulong bn)
{
	return md_addr(md, md_p2o(bn));
}

static inline struct zufs_dev_table *md_zdt(struct multi_devices *md)
{
	return md_t1_addr(md, 0);
}

static inline ulong md_t1_blocks(struct multi_devices *md)
{
	return le64_to_cpu(md_zdt(md)->s_t1_blocks);
}

static inline ulong md_t2_blocks(struct multi_devices *md)
{
	return le64_to_cpu(md_zdt(md)->s_t2_blocks);
}

static inline struct md_dev_info *md_bn_t2_dev(struct multi_devices *md,
					       ulong bn)
{
	return md->t2a.map[bn / md->t2a.bn_gcd];
}

static inline ulong md_t2_local_bn(struct multi_devices *md, ulong bn)
{
	struct md_dev_info *mdi = md_bn_t2_dev(md, bn);

	return bn - md_o2p(mdi->offset);
}

static inline void *md_addr_verify(struct multi_devices *md, ulong offset)
{
	if (unlikely(offset > md_p2o(md_t1_blocks(md)))) {
		zuf_dbg_err("offset=0x%lx > max=0x%llx\n",
			    offset, md_p2o(md_t1_blocks(md)));
		return NULL;
	}

	return md_addr(md, offset);
}

static inline const char *_bdev_name(struct block_device *bdev)
{
	return dev_name(&bdev->bd_part->__dev);
}

struct mdt_check {
	uint major_ver;
	uint minor_ver;
	u32  magic;

	void *holder;
	bool silent;
};

/* md.c */
struct zufs_dev_table *md_t2_mdt_read(struct block_device *bdev);
bool md_mdt_check(struct zufs_dev_table *msb, struct zufs_dev_table *main_msb,
		  struct block_device *bdev, struct mdt_check *mc);
struct multi_devices *md_alloc(size_t size);
int md_init(struct multi_devices *md, const char *dev_name,
	    struct mdt_check *mc, const char **dev_path);
void md_fini(struct multi_devices *md, struct block_device *s_bdev);
int md_set_sb(struct multi_devices *md, struct block_device *s_bdev, void *sb,
	      int silent);

struct zufs_ioc_pmem;
int md_numa_info(struct multi_devices *md, struct zufs_ioc_pmem *zi_pmem);

int md_t1_info_init(struct md_dev_info *mdi, bool silent);
void md_t1_info_fini(struct md_dev_info *mdi);

#endif
