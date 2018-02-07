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

#endif /* _LINUX_ZUFS_API_H */
