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

/* TODO: Someone forgot i_version for STATX_ attrs should send a patch to add it
 */
#define ZUFS_STATX_VERSION	0x40000000U

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

/*
 * Maximal count of links to a file
 */
#define ZUFS_LINK_MAX          32000
#define ZUFS_MAX_SYMLINK	PAGE_SIZE
#define ZUFS_NAME_LEN		255
#define ZUFS_READAHEAD_PAGES	8

/* All device sizes offsets must align on 2M */
#define ZUFS_ALLOC_MASK		(1024 * 1024 * 2 - 1)

/**
 * zufs dual port memory
 * This is a special type of offset to either memory or persistent-memory,
 * that is designed to be used in the interface mechanism between userspace
 * and kernel, and can be accessed by both. Note that user must use the
 * appropriate accessors to translate to a pointer.
 */
typedef __u64	zu_dpp_t;

/*
 * Structure of a ZUS inode.
 * This is all the inode fields
 */

/* zus_inode size */
#define ZUFS_INODE_SIZE 128    /* must be power of two */
#define ZUFS_INODE_BITS   7

struct zus_inode {
	__le32	i_flags;	/* Inode flags */
	__le16	i_mode;		/* File mode */
	__le16	i_nlink;	/* Links count */
	__le64	i_size;		/* Size of data in bytes */
/* 16*/	struct __zi_on_disk_desc {
		__le64	a[2];
	}	i_on_disk;	/* FS-specific on disc placement */
/* 32*/	__le64	i_blocks;
	__le64	i_mtime;	/* Inode/data Modification time */
	__le64	i_ctime;	/* Inode/data Changed time */
	__le64	i_atime;	/* Data Access time */
/* 64 - cache-line boundary */
	__le64	i_ino;		/* Inode number */
	__le32	i_uid;		/* Owner Uid */
	__le32	i_gid;		/* Group Id */
	__le64	i_xattr;	/* FS-specific Extended attribute block */
	__le64	i_generation;	/* File version (for NFS) */
/* 96*/	union {
		__le32	i_rdev;		/* special-inode major/minor etc ...*/
		u8	i_symlink[32];	/* if i_size < sizeof(i_symlink) */
		__le64	i_sym_sno;	/* FS-specific symlink placement */
		struct  _zu_dir {
			__le64  parent;
		}	i_dir;
	};
	/* Total ZUFS_INODE_SIZE bytes always */
};

#define ZUFS_SB_SIZE 2048       /* must be power of two */

/* device table s_flags */
#define		ZUFS_SHADOW	(1UL << 4)	/* simulate cpu cache */

#define test_msb_opt(msb, opt)	(le64_to_cpu(msb->s_flags) & opt)

#define ZUFS_DEV_NUMA_SHIFT		60
#define ZUFS_DEV_BLOCKS_MASK		0x0FFFFFFFFFFFFFFF

struct md_dev_id {
	uuid_le	uuid;
	__le64	blocks;
} __packed;

static inline __u64 __dev_id_blocks(struct md_dev_id *dev)
{
	return le64_to_cpu(dev->blocks) & ZUFS_DEV_BLOCKS_MASK;
}

static inline int __dev_id_nid(struct md_dev_id *dev)
{
	return (int)(le64_to_cpu(dev->blocks) >> ZUFS_DEV_NUMA_SHIFT);
}

/* 64 is the nicest number to still fit when the ZDT is 2048 and 6 bits can
 * fit in page struct for address to block translation.
 */
#define MD_DEV_MAX   64

struct md_dev_list {
	__le16		   id_index;	/* index of current dev in list */
	__le16		   t1_count;	/* # of t1 devs */
	__le16		   t2_count;	/* # of t2 devs (after t1_count) */
	__le16		   reserved;	/* align to 64 bit */
	struct md_dev_id dev_ids[MD_DEV_MAX];
} __attribute__((aligned(64)));

/*
 * Structure of the on disk zufs device table
 * NOTE: zufs_dev_table is always of size ZUFS_SB_SIZE. These below are the
 *   currently defined/used members in this version.
 *   TODO: remove the s_ from all the fields
 */
struct zufs_dev_table {
	/* static fields. they never change after file system creation.
	 * checksum only validates up to s_start_dynamic field below
	 */
	__le16		s_sum;              /* checksum of this sb */
	__le16		s_version;          /* zdt-version */
	__le32		s_magic;            /* magic signature */
	uuid_le		s_uuid;		    /* 128-bit uuid */
	__le64		s_flags;
	__le64		s_t1_blocks;
	__le64		s_t2_blocks;

	struct md_dev_list s_dev_list;

	char		s_start_dynamic[0];

	/* all the dynamic fields should go here */
	__le64		s_mtime;		/* mount time */
	__le64		s_wtime;		/* write time */
};

static inline int msb_major_version(struct zufs_dev_table *msb)
{
	return le16_to_cpu(msb->s_version) / ZUFS_MINORS_PER_MAJOR;
}

static inline int msb_minor_version(struct zufs_dev_table *msb)
{
	return le16_to_cpu(msb->s_version) % ZUFS_MINORS_PER_MAJOR;
}

#define ZUFS_SB_STATIC_SIZE(ps) ((u64)&ps->s_start_dynamic - (u64)ps)

/* xattr types */
enum {	X_F_SECURITY    = 1,
	X_F_SYSTEM      = 2,
	X_F_TRUSTED     = 3,
	X_F_USER        = 4,
};

struct tozu_xattr {
	__le64	next;
	__le16	name_length;
	__le16	value_size;
	u8	type;
	u8	res1[3];
	char	data[0];
} __packed;

struct tozu_acl {
	__le16	tag;
	__le16	perm;
	__le32	id;
} __packed;

/* ~~~~~ special tozu ioctl commands ~~~~~ */
struct tozu_fadvise {
	__u64	offset;
	__u64	length;		/* if 0 all file */
	__u64	flags;
} __packed;

#define ZUFS_IOC_FADVISE	_IOW('S', 2, struct tozu_fadvise)

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

/* pmem  */
struct zufs_ioc_pmem {
	/* Set by zus */
	struct zufs_ioc_hdr hdr;
	__u32 pmem_kern_id;

	/* Returned to zus */
	__u64 pmem_total_blocks;
	__u32 max_nodes;
	__u32 active_pmem_nodes;
	struct zufs_pmem_info {
		int sections;
		struct zufs_pmem_sec {
			__u32 length;
			__u16 numa_id;
			__u16 numa_index;
		} secs[MD_DEV_MAX];
	} pmem;

	/* Variable length array mapping A CPU to the proper active pmem to use.
	 * ZUS starts with 4k if to small hdr.err === ETOSMALL and
	 * max_cpu_id set for the needed amount.
	 *
	 * Careful a user_mode pointer if not needed by server set to NULL
	 *
	 * @max_cpu_id is set by server to say how much space at numa_info,
	 * Kernel returns the actual active CPUs
	 */
	struct zufs_numa_info {
		__u32 max_cpu_id;
		__u32 pad;
		struct zufs_cpu_info {
			__u32 numa_id;
			__u32 numa_index;
		} numa_id_map[];
	} *numa_info;
};
/* GRAB is never ungrabed umount or file close cleans it all */
#define ZU_IOC_GRAB_PMEM	_IOWR('S', 12, struct zufs_ioc_init)

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
	ZUS_OP_STATFS,

	ZUS_OP_NEW_INODE,
	ZUS_OP_FREE_INODE,
	ZUS_OP_EVICT_INODE,

	ZUS_OP_LOOKUP,
	ZUS_OP_ADD_DENTRY,
	ZUS_OP_REMOVE_DENTRY,
	ZUS_OP_RENAME,
	ZUS_OP_READDIR,
	ZUS_OP_CLONE,
	ZUS_OP_COPY,

	ZUS_OP_READ,
	ZUS_OP_WRITE,
	ZUS_OP_GET_BLOCK,
	ZUS_OP_GET_SYMLINK,
	ZUS_OP_SETATTR,
	ZUS_OP_UPDATE_TIME,
	ZUS_OP_SYNC,
	ZUS_OP_FALLOCATE,
	ZUS_OP_LLSEEK,

	ZUS_OP_BREAK,		/* Kernel telling Server to exit */
	ZUS_OP_MAX_OPT,
};

/* ZUS_OP_STATFS */
struct zufs_ioc_statfs {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_sb_info *zus_sbi;

	/* OUT */
	struct statfs64 statfs_out;
};

/* zufs_ioc_new_inode flags: */
enum zi_flags {
	ZI_TMPFILE = 1,		/* for new_inode */
	ZI_LOOKUP_RACE = 1,	/* for evict */
};

struct zufs_str {
	__u8 len;
	char name[ZUFS_NAME_LEN];
};

/* ZUS_OP_NEW_INODE */
struct zufs_ioc_new_inode {
	struct zufs_ioc_hdr hdr;
	 /* IN */
	struct zus_inode zi;
	struct zus_inode_info *dir_ii; /* If mktmp this is the root */
	struct zufs_str str;
	__u64 flags;

	 /* OUT */
	zu_dpp_t _zi;
	struct zus_inode_info *zus_ii;
};

/* ZUS_OP_FREE_INODE, ZUS_OP_EVICT_INODE */
struct zufs_ioc_evict_inode {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *zus_ii;
	__u64 flags;
};

/* ZUS_OP_LOOKUP */
struct zufs_ioc_lookup {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *dir_ii;
	struct zufs_str str;

	 /* OUT */
	zu_dpp_t _zi;
	struct zus_inode_info *zus_ii;
};

/* ZUS_OP_ADD_DENTRY, ZUS_OP_REMOVE_DENTRY */
struct zufs_ioc_dentry {
	struct zufs_ioc_hdr hdr;
	struct zus_inode_info *zus_ii; /* IN */
	struct zus_inode_info *zus_dir_ii; /* IN */
	struct zufs_str str; /* IN */
	__u64 ino; /* OUT - only for lookup */
};

/* ZUS_OP_RENAME */
struct zufs_ioc_rename {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *old_dir_ii;
	struct zus_inode_info *new_dir_ii;
	struct zus_inode_info *old_zus_ii;
	struct zus_inode_info *new_zus_ii;
	struct zufs_str old_d_str;
	struct zufs_str new_d_str;
	__le64 time;
};

/* ZUS_OP_READDIR */
struct zufs_ioc_readdir {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *dir_ii;
	loff_t pos;

	/* OUT */
	__u8	more;
};

struct zufs_dir_entry {
	__le64 ino;
	struct {
		unsigned	type	: 8;
		ulong		pos	: 56;
	};
	struct zufs_str zstr;
};

struct zufs_readdir_iter {
	void *__zde, *last;
	struct zufs_ioc_readdir *ioc_readdir;
};

enum {E_ZDE_HDR_SIZE =
	offsetof(struct zufs_dir_entry, zstr) + offsetof(struct zufs_str, name),
};

static inline void zufs_readdir_iter_init(struct zufs_readdir_iter *rdi,
					  struct zufs_ioc_readdir *ioc_readdir,
					  void *app_ptr)
{
	rdi->__zde = app_ptr;
	rdi->last = app_ptr + ioc_readdir->hdr.len;
	rdi->ioc_readdir = ioc_readdir;
	ioc_readdir->more = false;
}

static inline uint zufs_dir_entry_len(__u8 name_len)
{
	return ALIGN(E_ZDE_HDR_SIZE + name_len, sizeof(__u64));
}

static inline
struct zufs_dir_entry *zufs_next_zde(struct zufs_readdir_iter *rdi)
{
	struct zufs_dir_entry *zde = rdi->__zde;
	uint len;

	if (rdi->last <= rdi->__zde + E_ZDE_HDR_SIZE)
		return NULL;
	if (zde->zstr.len == 0)
		return NULL;
	len = zufs_dir_entry_len(zde->zstr.len);
	if (rdi->last <= rdi->__zde + len)
		return NULL;

	rdi->__zde += len;
	return zde;
}

static inline bool zufs_zde_emit(struct zufs_readdir_iter *rdi, __u64 ino,
				 __u8 type, __u64 pos, const char *name,
				 __u8 len)
{
	struct zufs_dir_entry *zde = rdi->__zde;

	if (rdi->last <= rdi->__zde + zufs_dir_entry_len(len)) {
		rdi->ioc_readdir->more = true;
		return false;
	}

	rdi->ioc_readdir->more = 0;
	zde->ino = ino;
	zde->type = type;
	/*ASSERT(0 == (pos && (1 << 56 - 1)));*/
	zde->pos = pos;
	strncpy(zde->zstr.name, name, len);
	zde->zstr.len = len;
	zufs_next_zde(rdi);

	return true;
}

/* ZUS_OP_READ/ZUS_OP_WRITE */
struct zufs_ioc_IO {
	struct zufs_ioc_hdr hdr;
	struct zus_inode_info *zus_ii; /* IN */

	__u64 filepos;
};

enum {
	ZUFS_GBF_RESERVED = 1,
	ZUFS_GBF_NEW = 2,
};

/* ZUS_OP_GET_BLOCK */
struct zufs_ioc_get_block {
	struct zufs_ioc_hdr hdr;
	 /* IN */
	struct zus_inode_info *zus_ii;
	__u64 index; /* page index in file */
	__u64 rw; /* Some flags + READ or WRITE */

	/* OUT */
	zu_dpp_t pmem_bn; /* zero return means: map a hole */
	__u64 ret_flags;  /* One of ZUFS_GBF_XXX */
};

/* ZUS_OP_GET_SYMLINK */
struct zufs_ioc_get_link {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *zus_ii;

	/* OUT */
	zu_dpp_t _link;
};

/* ZUS_OP_SETATTR */
struct zufs_ioc_attr {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *zus_ii;
	__u64 truncate_size;
	__u32 zuf_attr;
	__u32 pad;
};

/* ZUS_OP_ISYNC, ZUS_OP_FALLOCATE */
struct zufs_ioc_range {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *zus_ii;
	__u64 offset, length;
	__u32 opflags;
	__u32 pad;

	/* OUT */
	__u64 write_unmapped;
};

/* ZUS_OP_CLONE */
struct zufs_ioc_clone {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *src_zus_ii;
	struct zus_inode_info *dst_zus_ii;
	__u64 pos_in, pos_out;
	__u64 len;
};

/* ZUS_OP_LLSEEK */
struct zufs_ioc_seek {
	struct zufs_ioc_hdr hdr;
	/* IN */
	struct zus_inode_info *zus_ii;
	__u64 offset_in;
	__u32 whence;
	__u32 pad;

	/* OUT */
	__u64 offset_out;
};

#endif /* _LINUX_ZUFS_API_H */
