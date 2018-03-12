/*
 * BRIEF DESCRIPTION
 *
 * Inode methods (allocate/free/read/write).
 *
 * Copyright (c) 2018 NetApp Inc. All rights reserved.
 *
 * ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
 *
 * Authors:
 *	Boaz Harrosh <boazh@netapp.com>
 *	Sagi Manole <sagim@netapp.com>"
 */

#include "zuf.h"

struct inode *zuf_iget(struct super_block *sb, struct zus_inode_info *zus_ii,
		       zu_dpp_t _zi, bool *exist)
{
	return ERR_PTR(-ENOMEM);
}

void zuf_evict_inode(struct inode *inode)
{
}

int zuf_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	/* write_inode should never be called because we always keep our inodes
	 * clean. So let us know if write_inode ever gets called.
	 */

	/* d_tmpfile() does a mark_inode_dirty so only complain on regular files
	 * TODO: How? Every thing off for now
	 * WARN_ON(inode->i_nlink);
	 */

	return 0;
}

/* This function is called by msync(), fsync() && sync_fs(). */
int zuf_isync(struct inode *inode, loff_t start, loff_t end, int datasync)
{
	return 0;
}
