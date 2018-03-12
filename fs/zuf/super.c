/*
 * BRIEF DESCRIPTION
 *
 * Super block operations.
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

struct dentry *zuf_mount(struct file_system_type *fs_type, int flags,
			 const char *dev_name, void *data)
{
	return ERR_PTR(-ENOTSUPP);
}
