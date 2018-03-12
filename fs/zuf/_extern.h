/*
 * Copyright (c) 2018 NetApp Inc. All rights reserved.
 *
 * ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
 *
 * Authors:
 *	Boaz Harrosh <boazh@netapp.com>
 *	Sagi Manole <sagim@netapp.com>"
 */

#ifndef __ZUF_EXTERN_H__
#define __ZUF_EXTERN_H__
/*
 * DO NOT INCLUDE this file directly, it is included by zuf.h
 * It is here because zuf.h got to big
 */

/*
 * extern functions declarations
 */

/* super.c */
struct dentry *zuf_mount(struct file_system_type *fs_type, int flags,
			 const char *dev_name, void *data);

/* zuf-core.c */
int zufs_zts_init(struct zuf_root_info *zri); /* Some private types in core */
void zufs_zts_fini(struct zuf_root_info *zri);

long zufs_ioc(struct file *filp, unsigned int cmd, ulong arg);
int zufs_dispatch_mount(struct zuf_root_info *zri, struct zus_fs_info *zus_zfi,
			struct zufs_ioc_mount *zim);
int zufs_dispatch_umount(struct zuf_root_info *zri,
			 struct zus_sb_info *zus_sbi);

int zufs_dispatch(struct zuf_root_info *zri, struct zufs_ioc_hdr *hdr,
		  struct page **pages, uint nump);

int zuf_zt_mmap(struct file *file, struct vm_area_struct *vma);

void zufs_zt_release(struct file *filp);
void zufs_mounter_release(struct file *filp);

/* zuf-root.c */
int zuf_register_fs(struct super_block *sb, struct zufs_ioc_register_fs *rfs);

#endif	/*ndef __ZUF_EXTERN_H__*/
