/*
 * zufs_api.h:
 *	ZUFS (Zero-copy User-mode File System) is:
 *		zuf (Zero-copy User-mode Feeder (Kernel)) +
 *		zus (Zero-copy User-mode Server (daemon))
 *
 * Copyright (c) 2018 NetApp Inc. All rights reserved.
 *
 * ZUFS-License: GPL-2.0 OR BSD-3-Clause. See module.c for LICENSE details.
 *
 * Authors:
 *	Boaz Harrosh <boazh@netapp.com>
 *	Sagi Manole <sagim@netapp.com>"
 */
#ifndef _LINUX_ZUFS_API_H
#define _LINUX_ZUFS_API_H

#include <linux/types.h>
#include <linux/uuid.h>
#include <stddef.h>
#include <asm/statfs.h>

/*
 * Version rules:
 *   This is the zus-to-zuf API version. And not the Filesystem
 * on disk structures versions. These are left to the FS-plugging
 * to supply and check.
 * Specifically any of the API structures and constants found in this
 * file.
 * If the changes are made in a way backward compatible with old
 * user-space, MINOR is incremented. Else MAJOR is incremented.
 *
 * We believe that the zus Server application comes with the
 * Distro and should be dependent on the Kernel package.
 * The more stable ABI is between the zus Server and its FS plugins.
 * Because of the intimate relationships in the zuf-core behavior
 * We would also like zus Server to be signed by the running Kernel's
 * make crypto key and checked before load because of the Security
 * nature of an FS provider.
 */
#define ZUFS_MINORS_PER_MAJOR	1024
#define ZUFS_MAJOR_VERSION 1
#define ZUFS_MINOR_VERSION 0

/* User space compatibility definitions */
#ifndef __KERNEL__

#include <stdint.h>
#include <stdbool.h>

#define u8 uint8_t
#define umode_t uint16_t

#define le16_to_cpu	le16toh
#define le64_to_cpu	le64toh

#define PAGE_SHIFT     12
#define PAGE_SIZE      (1 << PAGE_SHIFT)

#ifndef __packed
#	define __packed __attribute__((packed))
#endif

#define ALIGN(x, a)		ALIGN_MASK(x, (typeof(x))(a) - 1)
#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))

#endif /*  ndef __KERNEL__ */

/**
 * zufs dual port memory
 * This is a special type of offset to either memory or persistent-memory,
 * that is designed to be used in the interface mechanism between userspace
 * and kernel, and can be accessed by both. Note that user must use the
 * appropriate accessors to translate to a pointer.
 */
typedef __u64	zu_dpp_t;

/* ~~~~~ ZUFS API ioctl commands ~~~~~ */
enum {
	ZUS_API_MAP_MAX_PAGES	= 1024,
	ZUS_API_MAP_MAX_SIZE	= ZUS_API_MAP_MAX_PAGES * PAGE_SIZE,
};

struct zufs_ioc_hdr {
	__u32 err;	/* IN/OUT must be first */
	__u16 in_start;	/* Not used always 0 */
	__u16 in_len;	/* How much to be copied *to* user mode */
	__u16 out_start;/* start of output parameters */
	__u16 out_len;	/* How much to be copied *from* user mode */
	__u32 operation;/* One of e_zufs_operation */
	__u32 offset;	/* Start of user buffer in ZT mmap */
	__u32 len;	/* Len of user buffer in ZT mmap */
};

/* Register FS */
/* A cookie from user-mode given in register_fs_info */
struct zus_fs_info;
struct zufs_ioc_register_fs {
	struct zufs_ioc_hdr hdr;
	struct zus_fs_info *zus_zfi;
	struct register_fs_info {
		/* IN */
		char fsname[16];	/* Only 4 chars and a NUL please      */
		__u32 FS_magic;         /* This is the FS's version && magic  */
		__u32 FS_ver_major;	/* on disk, not the zuf-to-zus version*/
		__u32 FS_ver_minor;	/* (See also struct zufs_dev_table)   */

		__u8 acl_on;
		__u8 notused[3];
		__u64 dt_offset;

		__u32 s_time_gran;
		__u32 def_mode;
		__u64 s_maxbytes;

	} rfi;
};
#define ZU_IOC_REGISTER_FS	_IOWR('S', 10, struct zufs_ioc_register_fs)

/* A cookie from user-mode returned by mount */
struct zus_sb_info;

/* zus cookie per inode */
struct zus_inode_info;

/* mount / umount */
struct  zufs_ioc_mount {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_fs_info *zus_zfi;
	uint num_cpu;
	uint pmem_kern_id;
	__u8 is_umounting;

	/* OUT */
	struct zus_sb_info *zus_sbi;
	/* mount is also iget of root */
	struct zus_inode_info *zus_ii;
	zu_dpp_t _zi;

	/* More mount info */
	__u32 s_blocksize_bits;
};
#define ZU_IOC_MOUNT	_IOWR('S', 12, struct zufs_ioc_mount)

/* ZT init */
struct zufs_ioc_init {
	struct zufs_ioc_hdr hdr;
	ulong affinity;	/* IN */
};
#define ZU_IOC_INIT_THREAD	_IOWR('S', 20, struct zufs_ioc_init)

/* break_all (Server telling kernel to clean) */
struct zufs_ioc_break_all {
	struct zufs_ioc_hdr hdr;
};
#define ZU_IOC_BREAK_ALL	_IOWR('S', 22, struct zufs_ioc_break_all)

enum { ZUFS_MAX_COMMAND_BUFF = (PAGE_SIZE - sizeof(struct zufs_ioc_hdr)) };
struct zufs_ioc_wait_operation {
	struct zufs_ioc_hdr hdr;
	char opt_buff[ZUFS_MAX_COMMAND_BUFF];
};
#define ZU_IOC_WAIT_OPT		_IOWR('S', 21, struct zufs_ioc_wait_operation)

/* ~~~ all the permutations of zufs_ioc_wait_operation ~~~ */
/* These are the possible operations sent from Kernel to the Server in the
 * return of the ZU_IOC_WAIT_OPT.
 */
enum e_zufs_operation {
	ZUS_OP_NULL = 0,

	ZUS_OP_BREAK,		/* Kernel telling Server to exit */
	ZUS_OP_MAX_OPT,
};

#endif /* _LINUX_ZUFS_API_H */
