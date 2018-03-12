/*
 * BRIEF DESCRIPTION
 *
 * Definitions for the ZUF filesystem.
 *
 * Copyright (c) 2018 NetApp Inc. All rights reserved.
 *
 * ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
 *
 * Authors:
 *	Boaz Harrosh <boazh@netapp.com>
 *	Sagi Manole <sagim@netapp.com>"
 */

#ifndef __ZUF_H
#define __ZUF_H

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/xattr.h>
#include <linux/exportfs.h>
#include <linux/page_ref.h>

#include "zus_api.h"

#include "relay.h"
#include "_pr.h"

enum zlfs_e_special_file {
	zlfs_e_zt = 1,
	zlfs_e_mout_thread,
	zlfs_e_pmem,
};

struct zuf_special_file {
	enum zlfs_e_special_file type;
};

/* This is the zuf-root.c mini filesystem */
struct zuf_root_info {
	struct __mount_thread_info {
		struct zuf_special_file zsf;
		spinlock_t lock;
		struct relay relay;
		struct zufs_ioc_mount *zim;
		struct file *file;
	} mount;

	ulong next_ino;

	uint _max_zts;
	struct zufs_thread *_all_zt;

	struct super_block *sb;
	struct list_head fst_list;

	uint next_pmem_id;
	struct list_head pmem_list;
};

static inline struct zuf_root_info *ZRI(struct super_block *sb)
{
	struct zuf_root_info *zri = sb->s_fs_info;

	WARN_ON(zri->sb != sb);
	return zri;
}

struct zuf_fs_type {
	struct file_system_type vfs_fst;
	struct zus_fs_info	*zus_zfi;
	struct register_fs_info rfi;
	struct zuf_root_info *zri;

	struct list_head list;
};

static inline void zuf_add_fs_type(struct zuf_root_info *zri,
				   struct zuf_fs_type *zft)
{
	/* Unlocked for now only one mount-thread with zus */
	list_add(&zft->list, &zri->fst_list);
}

/* Keep this include last thing in file */
#include "_extern.h"

#endif /* __ZUF_H */
