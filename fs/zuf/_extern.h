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

/* directory.c */
int zuf_add_dentry(struct inode *dir, struct qstr *str,
		   struct inode *inode, bool rename);
int zuf_remove_dentry(struct inode *dir, struct qstr *str);

/* inode.c */
int zuf_evict_dispatch(struct super_block *sb, struct zus_inode_info *zus_ii,
		       int operation);
struct inode *zuf_iget(struct super_block *sb, struct zus_inode_info *zus_ii,
		       zu_dpp_t _zi, bool *exist);
void zuf_evict_inode(struct inode *inode);
struct inode *zuf_new_inode(struct inode *dir, umode_t mode,
			    const struct qstr *qstr, const char *symname,
			    ulong rdev_or_isize, bool tmpfile);
int zuf_write_inode(struct inode *inode, struct writeback_control *wbc);
int zuf_update_time(struct inode *inode, struct timespec *time, int flags);
int zuf_setattr(struct dentry *dentry, struct iattr *attr);
int zuf_getattr(const struct path *path, struct kstat *stat,
		 u32 request_mask, unsigned int flags);
void zuf_set_inode_flags(struct inode *inode, struct zus_inode *zi);
bool zuf_dir_emit(struct super_block *sb, struct dir_context *ctx,
		  ulong ino, const char *name, int length);

/* symlink.c */
uint zuf_prepare_symname(struct zufs_ioc_new_inode *ioc_new_inode,
			const char *symname, ulong len, struct page *pages[2]);

/* file.c */
int zuf_isync(struct inode *inode, loff_t start, loff_t end, int datasync);

/* super.c */
int zuf_init_inodecache(void);
void zuf_destroy_inodecache(void);

void zuf_sync_inc(struct inode *inode);
void zuf_sync_dec(struct inode *inode, ulong write_unmapped);

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

/* t1.c */
int zuf_pmem_mmap(struct file *file, struct vm_area_struct *vma);

/*
 * Inodes and files operations
 */

/* dir.c */
extern const struct file_operations zuf_dir_operations;

/* file.c */
extern const struct inode_operations zuf_file_inode_operations;
extern const struct file_operations zuf_file_operations;

/* inode.c */
extern const struct address_space_operations zuf_aops;

/* namei.c */
extern const struct inode_operations zuf_dir_inode_operations;
extern const struct inode_operations zuf_special_inode_operations;

/* symlink.c */
extern const struct inode_operations zuf_symlink_inode_operations;

#endif	/*ndef __ZUF_EXTERN_H__*/
