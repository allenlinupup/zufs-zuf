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

#include <linux/blkdev.h>
#include <linux/pfn_t.h>
#include <linux/crc16.h>
#include <linux/uuid.h>
#include <linux/gcd.h>
#include <linux/dax.h>

#include "_pr.h"
#include "md.h"
#include "t2.h"
#include "zus_api.h"

/* length of uuid dev path /dev/disk/by-uuid/<uuid> */
#define PATH_UUID	64

const fmode_t _g_mode = FMODE_READ | FMODE_WRITE | FMODE_EXCL;

/* allocate space for and copy an existing uuid */
static char *_uuid_path(uuid_le *uuid)
{
	char path[PATH_UUID];

	sprintf(path, "/dev/disk/by-uuid/%pUb", uuid);
	return kstrdup(path, GFP_KERNEL);
}

static int _bdev_get_by_path(const char *path, struct block_device **bdev,
			     void *holder)
{
	/* The owner of the device is the pointer that will hold it. This
	 * protects from same device mounting on two super-blocks as well
	 * as same device being repeated twice.
	 */
	*bdev = blkdev_get_by_path(path, _g_mode, holder);
	if (IS_ERR(*bdev)) {
		int err = PTR_ERR(*bdev);
		*bdev = NULL;
		return err;
	}
	return 0;
}

static void _bdev_put(struct block_device **bdev, struct block_device *s_bdev)
{
	if (*bdev) {
		if (!s_bdev || *bdev != s_bdev)
			blkdev_put(*bdev, _g_mode);
		*bdev = NULL;
	}
}

static int ___bdev_get_by_uuid(struct block_device **bdev, uuid_le *uuid,
			       void *holder, bool silent, const char *msg,
			       const char *f, int l)
{
	char *path = NULL;
	int err;

	path = _uuid_path(uuid);
	err = _bdev_get_by_path(path, bdev, holder);
	if (unlikely(err))
		zuf_err_cnd(silent, "[%s:%d] %s path=%s =>%d\n",
			     f, l, msg, path, err);

	kfree(path);
	return err;
}

#define _bdev_get_by_uuid(bdev, uuid, holder, msg) \
	___bdev_get_by_uuid(bdev, uuid, holder, silent, msg, __func__, __LINE__)

static bool _main_bdev(struct block_device *bdev)
{
	if (bdev->bd_super && bdev->bd_super->s_bdev == bdev)
		return true;
	return false;
}

short md_calc_csum(struct zufs_dev_table *msb)
{
	uint n = ZUFS_SB_STATIC_SIZE(msb) - sizeof(msb->s_sum);

	return crc16(~0, (__u8 *)&msb->s_version, n);
}

/* ~~~~~~~ mdt related functions ~~~~~~~ */

struct zufs_dev_table *md_t2_mdt_read(struct block_device *bdev)
{
	int err;
	struct page *page;
	/* t2 interface works for all block devices */
	struct multi_devices *md;
	struct md_dev_info *mdi;

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (unlikely(!md))
		return ERR_PTR(-ENOMEM);

	md->t2_count = 1;
	md->devs[0].bdev = bdev;
	mdi = &md->devs[0];
	md->t2a.map = &mdi;
	md->t2a.bn_gcd = 1; /*Does not matter only must not be zero */

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		zuf_dbg_err("!!! failed to alloc page\n");
		err = -ENOMEM;
		goto out;
	}

	err = t2_readpage(md, 0, page);
	if (err) {
		zuf_dbg_err("!!! t2_readpage err=%d\n", err);
		__free_page(page);
	}
out:
	kfree(md);
	return err ? ERR_PTR(err) : page_address(page);
}

static bool _csum_mismatch(struct zufs_dev_table *msb, int silent)
{
	ushort crc = md_calc_csum(msb);

	if (msb->s_sum == cpu_to_le16(crc))
		return false;

	zuf_warn_cnd(silent, "expected(0x%x) != s_sum(0x%x)\n",
		      cpu_to_le16(crc), msb->s_sum);
	return true;
}

static bool _uuid_le_equal(uuid_le *uuid1, uuid_le *uuid2)
{
	return (memcmp(uuid1, uuid2, sizeof(uuid_le)) == 0);
}

bool md_mdt_check(struct zufs_dev_table *msb,
		  struct zufs_dev_table *main_msb, struct block_device *bdev,
		  struct mdt_check *mc)
{
	struct zufs_dev_table *msb2 = (void *)msb + ZUFS_SB_SIZE;
	struct md_dev_id *dev_id;
	ulong bdev_size, super_size;

	BUILD_BUG_ON(ZUFS_SB_STATIC_SIZE(msb) & (SMP_CACHE_BYTES - 1));

	/* Do sanity checks on the superblock */
	if (le32_to_cpu(msb->s_magic) != mc->magic) {
		if (le32_to_cpu(msb2->s_magic) != mc->magic) {
			zuf_warn_cnd(mc->silent,
				     "Can't find a valid partition\n");
			return false;
		}

		zuf_warn_cnd(mc->silent,
			     "Magic error in super block: using copy\n");
		/* Try to auto-recover the super block */
		memcpy_flushcache(msb, msb2, sizeof(*msb));
	}

	if ((mc->major_ver != msb_major_version(msb)) ||
	    (mc->minor_ver < msb_minor_version(msb))) {
		zuf_warn_cnd(mc->silent,
			     "mkfs-mount versions mismatch! %d.%d != %d.%d\n",
			     msb_major_version(msb), msb_minor_version(msb),
			     mc->major_ver, mc->minor_ver);
		return false;
	}

	if (_csum_mismatch(msb, mc->silent)) {
		if (_csum_mismatch(msb2, mc->silent)) {
			zuf_warn_cnd(mc->silent,
				     "checksum error in super block\n");
			return false;
		}

		zuf_warn_cnd(mc->silent,
			     "crc16 error in super block: using copy\n");
		/* Try to auto-recover the super block */
		memcpy_flushcache(msb, msb2, sizeof(*msb));
	}

	if (main_msb && !_uuid_le_equal(&main_msb->s_uuid, &msb->s_uuid)) {
		zuf_warn_cnd(mc->silent,
			     "uuids do not match main_msb=%pUb msb=%pUb\n",
			     &main_msb->s_uuid, &msb->s_uuid);
		return false;
	}

	/* check t1 device size */
	bdev_size = i_size_read(bdev->bd_inode);
	dev_id = &msb->s_dev_list.dev_ids[msb->s_dev_list.id_index];
	super_size = md_p2o(__dev_id_blocks(dev_id));
	if (unlikely(!super_size || super_size & ZUFS_ALLOC_MASK)) {
		zuf_warn_cnd(mc->silent, "super_size(0x%lx) ! 2_M aligned\n",
			      super_size);
		return false;
	}

	if (unlikely(super_size > bdev_size)) {
		zuf_warn_cnd(mc->silent,
			     "bdev_size(0x%lx) too small expected 0x%lx\n",
			     bdev_size, super_size);
		return false;
	} else if (unlikely(super_size < bdev_size)) {
		zuf_dbg_err("Note msb->size=(0x%lx) < bdev_size(0x%lx)\n",
			      super_size, bdev_size);
	}

	return true;
}


int md_set_sb(struct multi_devices *md, struct block_device *s_bdev,
	      void *sb, int silent)
{
	struct md_dev_info *mdi = md_dev_info(md, md->dev_index);
	int i;

	mdi->bdev = s_bdev;

	for (i = 0; i < md->t1_count; ++i) {
		struct md_dev_info *mdi = md_t1_dev(md, i);

		if (mdi->bdev->bd_super && (mdi->bdev->bd_super != sb)) {
			zuf_warn_cnd(silent,
				"!!! %s already mounted on a different FS => -EBUSY\n",
				_bdev_name(mdi->bdev));
			return -EBUSY;
		}

		mdi->bdev->bd_super = sb;
	}

	return 0;
}

void md_fini(struct multi_devices *md, struct block_device *s_bdev)
{
	int i;

	kfree(md->t2a.map);
	kfree(md->t1a.map);

	for (i = 0; i < md->t1_count + md->t2_count; ++i) {
		struct md_dev_info *mdi = md_dev_info(md, i);

		if (mdi->bdev && !_main_bdev(mdi->bdev))
			mdi->bdev->bd_super = NULL;
		_bdev_put(&mdi->bdev, s_bdev);
	}

	kfree(md);
}


/* ~~~~~~~ Pre-mount operations ~~~~~~~ */

static int _get_device(struct block_device **bdev, const char *dev_name,
		       uuid_le *uuid, void *holder, int silent,
		       bool *bind_mount)
{
	int err;

	if (dev_name)
		err = _bdev_get_by_path(dev_name, bdev, holder);
	else
		err = _bdev_get_by_uuid(bdev, uuid, holder,
					"failed to get device");

	if (unlikely(err)) {
		zuf_err_cnd(silent,
			"failed to get device dev_name=%s uuid=%pUb err=%d\n",
			dev_name, uuid, err);
		return err;
	}

	if (bind_mount && _main_bdev(*bdev))
		*bind_mount = true;

	return 0;
}

static int _init_dev_info(struct md_dev_info *mdi, struct md_dev_id *id,
			  int index, u64 offset,
			  struct zufs_dev_table *main_msb,
			  struct mdt_check *mc, bool t1_dev,
			  int silent)
{
	struct zufs_dev_table *msb = NULL;
	int err = 0;

	if (mdi->bdev == NULL) {
		err = _get_device(&mdi->bdev, NULL, &id->uuid, mc->holder,
				  silent, NULL);
		if (unlikely(err))
			return err;
	}

	mdi->offset = offset;
	mdi->size = md_p2o(__dev_id_blocks(id));
	mdi->index = index;

	if (t1_dev) {
		struct page *dev_page;
		int end_of_dev_nid;

		err = md_t1_info_init(mdi, silent);
		if (unlikely(err))
			return err;

		if ((ulong)mdi->t1i.virt_addr & ZUFS_ALLOC_MASK) {
			zuf_warn_cnd(silent, "!!! unaligned device %s\n",
				      _bdev_name(mdi->bdev));
			return -EINVAL;
		}

		msb = mdi->t1i.virt_addr;

		dev_page = pfn_to_page(mdi->t1i.phys_pfn);
		mdi->nid = page_to_nid(dev_page);
		end_of_dev_nid = page_to_nid(dev_page + md_o2p(mdi->size - 1));

		if (mdi->nid != end_of_dev_nid)
			zuf_warn("pmem crosses NUMA boundaries");
	} else {
		msb = md_t2_mdt_read(mdi->bdev);
		if (IS_ERR(msb)) {
			zuf_err_cnd(silent,
				    "failed to read msb from t2 => %ld\n",
				    PTR_ERR(msb));
			return PTR_ERR(msb);
		}
		mdi->nid = __dev_id_nid(id);
	}

	if (!md_mdt_check(msb, main_msb, mdi->bdev, mc)) {
		zuf_err_cnd(silent, "device %s failed integrity check\n",
			     _bdev_name(mdi->bdev));
		err = -EINVAL;
		goto out;
	}

	return 0;

out:
	if (!(t1_dev || IS_ERR_OR_NULL(msb)))
		free_page((ulong)msb);
	return err;
}

static int _map_setup(struct multi_devices *md, ulong blocks, int dev_start,
		      struct md_dev_larray *larray)
{
	ulong map_size, bn_end;
	int i, dev_index = dev_start;

	map_size = blocks / larray->bn_gcd;
	larray->map = kcalloc(map_size, sizeof(*larray->map), GFP_KERNEL);
	if (!larray->map) {
		zuf_dbg_err("failed to allocate dev map\n");
		return -ENOMEM;
	}

	bn_end = md_o2p(md->devs[dev_index].size);
	for (i = 0; i < map_size; ++i) {
		if ((i * larray->bn_gcd) >= bn_end)
			bn_end += md_o2p(md->devs[++dev_index].size);
		larray->map[i] = &md->devs[dev_index];
	}

	return 0;
}

static int _md_init(struct multi_devices *md, struct mdt_check *mc,
		    struct md_dev_list *dev_list, int silent)
{
	struct zufs_dev_table *main_msb = NULL;
	u64 total_size = 0;
	int i, err;

	for (i = 0; i < md->t1_count; ++i) {
		struct md_dev_info *mdi = md_t1_dev(md, i);
		struct zufs_dev_table *dev_msb;

		err = _init_dev_info(mdi, &dev_list->dev_ids[i], i, total_size,
				     main_msb, mc, true, silent);
		if (unlikely(err))
			return err;

		/* apparently gcd(0,X)=X which is nice */
		md->t1a.bn_gcd = gcd(md->t1a.bn_gcd, md_o2p(mdi->size));
		total_size += mdi->size;

		dev_msb = md_t1_addr(md, i);
		if (!main_msb)
			main_msb = dev_msb;

		if (test_msb_opt(dev_msb, ZUFS_SHADOW))
			memcpy(mdi->t1i.virt_addr,
			       mdi->t1i.virt_addr + mdi->size, mdi->size);

		zuf_dbg_verbose("dev=%d %pUb %s v=%p pfn=%lu off=%lu size=%lu\n",
				 i, &dev_list->dev_ids[i].uuid,
				 _bdev_name(mdi->bdev), dev_msb,
				 mdi->t1i.phys_pfn, mdi->offset, mdi->size);
	}

	if (unlikely(le64_to_cpu(main_msb->s_t1_blocks) !=
						md_o2p(total_size))) {
		zuf_err_cnd(silent,
			"FS corrupted msb->t1_blocks(0x%llx) != total_size(0x%llx)\n",
			main_msb->s_t1_blocks, total_size);
		return -EIO;
	}

	err = _map_setup(md, le64_to_cpu(main_msb->s_t1_blocks), 0, &md->t1a);
	if (unlikely(err))
		return err;

	zuf_dbg_verbose("t1 devices=%d total_size=%llu segment_map=%lu\n",
			 md->t1_count, total_size,
			 md_o2p(total_size) / md->t1a.bn_gcd);

	if (md->t2_count == 0)
		return 0;

	/* Done with t1. Counting t2s */
	total_size = 0;
	for (i = 0; i < md->t2_count; ++i) {
		struct md_dev_info *mdi = md_t2_dev(md, i);

		err = _init_dev_info(mdi, &dev_list->dev_ids[md->t1_count + i],
				     md->t1_count + i, total_size, main_msb,
				     mc, false, silent);
		if (unlikely(err))
			return err;

		/* apparently gcd(0,X)=X which is nice */
		md->t2a.bn_gcd = gcd(md->t2a.bn_gcd, md_o2p(mdi->size));
		total_size += mdi->size;

		zuf_dbg_verbose("dev=%d %s off=%lu size=%lu\n", i,
				 _bdev_name(mdi->bdev), mdi->offset, mdi->size);
	}

	if (unlikely(le64_to_cpu(main_msb->s_t2_blocks) != md_o2p(total_size))) {
		zuf_err_cnd(silent,
			"FS corrupted msb_t2_blocks(0x%llx) != total_size(0x%llx)\n",
			main_msb->s_t2_blocks, total_size);
		return -EIO;
	}

	err = _map_setup(md, le64_to_cpu(main_msb->s_t2_blocks), md->t1_count,
			 &md->t2a);
	if (unlikely(err))
		return err;

	zuf_dbg_verbose("t2 devices=%d total_size=%llu segment_map=%lu\n",
			 md->t2_count, total_size,
			 md_o2p(total_size) / md->t2a.bn_gcd);

	return 0;
}

static int _load_dev_list(struct md_dev_list *dev_list, struct mdt_check *mc,
			  struct block_device *bdev, const char *dev_name,
			  int silent)
{
	struct zufs_dev_table *msb;
	int err = 0;

	msb = md_t2_mdt_read(bdev);
	if (IS_ERR(msb)) {
		zuf_err_cnd(silent,
			    "failed to read super block from %s; err=%ld\n",
			    dev_name, PTR_ERR(msb));
		err = PTR_ERR(msb);
		goto out;
	}

	if (!md_mdt_check(msb, NULL, bdev, mc)) {
		zuf_err_cnd(silent, "bad msb in %s\n", dev_name);
		err = -EINVAL;
		goto out;
	}

	*dev_list = msb->s_dev_list;

out:
	if (!IS_ERR_OR_NULL(msb))
		free_page((ulong)msb);

	return err;
}

int md_init(struct multi_devices *md, const char *dev_name,
	    struct mdt_check *mc, const char **dev_path)
{
	struct md_dev_list *dev_list;
	struct block_device *bdev;
	short id_index;
	bool bind_mount = false;
	int err;

	dev_list = kmalloc(sizeof(*dev_list), GFP_KERNEL);
	if (unlikely(!dev_list))
		return -ENOMEM;

	err = _get_device(&bdev, dev_name, NULL, mc->holder, mc->silent,
			  &bind_mount);
	if (unlikely(err))
		goto out2;

	err = _load_dev_list(dev_list, mc, bdev, dev_name, mc->silent);
	if (unlikely(err)) {
		_bdev_put(&bdev, NULL);
		goto out2;
	}

	id_index = le16_to_cpu(dev_list->id_index);
	if (bind_mount) {
		_bdev_put(&bdev, NULL);
		md->dev_index = id_index;
		goto out;
	}

	md->t1_count = le16_to_cpu(dev_list->t1_count);
	md->t2_count = le16_to_cpu(dev_list->t2_count);
	md->devs[id_index].bdev = bdev;

	if ((id_index != 0)) {
		err = _get_device(&md_t1_dev(md, 0)->bdev, NULL,
				  &dev_list->dev_ids[0].uuid, mc->holder,
				  mc->silent, &bind_mount);
		if (unlikely(err))
			goto out2;

		if (bind_mount)
			goto out;
	}

	if (md->t2_count) {
		int t2_index = md->t1_count;

		/* t2 is the primary device if given in mount, or the first
		 * mount specified it as primary device
		 */
		if (id_index != md->t1_count) {
			err = _get_device(&md_t2_dev(md, 0)->bdev, NULL,
					  &dev_list->dev_ids[t2_index].uuid,
					  mc->holder, mc->silent, &bind_mount);
			if (unlikely(err))
				goto out2;
		}
		md->dev_index = t2_index;
	}

out:
	if (md->dev_index != id_index)
		*dev_path = _uuid_path(&dev_list->dev_ids[md->dev_index].uuid);
	else
		*dev_path = kstrdup(dev_name, GFP_KERNEL);

	if (!bind_mount) {
		err = _md_init(md, mc, dev_list, mc->silent);
		if (unlikely(err))
			goto out2;
		_bdev_put(&md_dev_info(md, md->dev_index)->bdev, NULL);
	} else {
		md_fini(md, NULL);
	}

out2:
	kfree(dev_list);

	return err;
}

struct multi_devices *md_alloc(size_t size)
{
	uint s = max(sizeof(struct multi_devices), size);
	struct multi_devices *md = kzalloc(s, GFP_KERNEL);

	if (unlikely(!md))
		return ERR_PTR(-ENOMEM);
	return md;
}

int md_numa_info(struct multi_devices *md, struct zufs_ioc_pmem *zi_pmem)
{
	zi_pmem->pmem_total_blocks = md_t1_blocks(md);
#if 0
	if (max_cpu_id < sys_num_active_cpus) {
		max_cpu_id = sys_num_active_cpus;
		return -ETOSMALL;
	}

	max_cpu_id = sys_num_active_cpus;
	__u32 max_nodes;
	__u32 active_pmem_nodes;
	struct zufs_pmem_info {
		int sections;
		struct zufs_pmem_sec {
			__u32 length;
			__u16 numa_id;
			__u16 numa_index;
		} secs[ZUFS_DEV_MAX];
	} pmem;

	struct zufs_numa_info {
		__u32 max_cpu_id; // The below array size
		struct zufs_cpu_info {
			__u32 numa_id;
			__u32 numa_index;
		} numa_id_map[];
	} *numa_info;
	k_nf = kcalloc(max_cpu_id, sizeof(struct zufs_cpu_info), GFP_KERNEL);
	....
	copy_to_user(->numa_info, kn_f,
		     max_cpu_id * sizeof(struct zufs_cpu_info));
#endif
	return 0;
}

static int _check_da_ret(struct md_dev_info *mdi, long avail, bool silent)
{
	if (unlikely(avail < (long)mdi->size)) {
		if (0 < avail) {
			zuf_warn_cnd(silent,
				"Unsupported DAX device %s (range mismatch) => 0x%lx < 0x%lx\n",
				_bdev_name(mdi->bdev), avail, mdi->size);
			return -ERANGE;
		}
		zuf_warn_cnd(silent, "!!! %s direct_access return =>%ld\n",
			     _bdev_name(mdi->bdev), avail);
		return avail;
	}
	return 0;
}

int md_t1_info_init(struct md_dev_info *mdi, bool silent)
{
	pfn_t a_pfn_t;
	void *addr;
	long nrpages, avail;
	int id;

	mdi->t1i.dax_dev = fs_dax_get_by_host(_bdev_name(mdi->bdev));
	if (unlikely(!mdi->t1i.dax_dev))
		return -EOPNOTSUPP;

	id = dax_read_lock();

	nrpages = dax_direct_access(mdi->t1i.dax_dev, 0, md_o2p(mdi->size),
				    &addr, &a_pfn_t);
	dax_read_unlock(id);
	if (unlikely(nrpages <= 0)) {
		if (!nrpages)
			nrpages = -ERANGE;
		avail = nrpages;
	} else {
		avail = md_p2o(nrpages);
	}

	mdi->t1i.virt_addr = addr;
	mdi->t1i.phys_pfn = pfn_t_to_pfn(a_pfn_t);

	zuf_dbg_verbose("0x%lx 0x%llx\n",
			 (ulong)addr, a_pfn_t.val);

	return _check_da_ret(mdi, avail, silent);
}

void md_t1_info_fini(struct md_dev_info *mdi)
{
	fs_put_dax(mdi->t1i.dax_dev);
	mdi->t1i.dax_dev = NULL;
	mdi->t1i.virt_addr = NULL;
}
