/*
 * Copyright (C) 2014-2015, Western Digital Technologies, Inc. <copyrightagent@wdc.com>
 * SPDX License Identifier: GPL-2.0+
 *
 * This file is released under the GPL v2 or any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Shanghua Wang (shanghua.wang@wdc.com)
 *          Platform Development Group
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/device-mapper.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/kthread.h>
#include <linux/crc32.h>
#include <linux/gfp.h>
#include <linux/mutex.h>
#include <linux/math64.h>
#include <linux/version.h>
#include "smrsim_types.h"
#include "smrsim_ioctl.h"
#include "smrsim_kapi.h"
#include "smrsim_zerror.h"

#define CREATE_TRACE_POINTS
#include "smrsim_trace.h"

#define SMR_ZONE_SIZE_SHIFT_DEFAULT    16    /* number of blocks/zone   */
#define SMR_BLOCK_SIZE_SHIFT_DEFAULT   3     /* number of sectors/block */
#define SMR_PAGE_SIZE_SHIFT_DEFAULT    3     /* number of sectors/page  */
#define SMR_SECTOR_SIZE_SHIFT_DEFAULT  9     /* number of bytes/sector  */
#define SMR_OUT_OF_POLICY_PENALTY      4000  /* ms */
#define SMR_OUT_OF_POLICY_PENALTY_MAX  10000 /* ms */

#define SMR_MAX_CAPACITY               21474836480
static __u64   SMR_CAPACITY;           /* number of sectors */
static __u32   SMR_NUMZONES;
static __u32   SMR_NUMZONES_DEFAULT;
static __u32   SMR_ZONE_SIZE_SHIFT;
static __u32   SMR_BLOCK_SIZE_SHIFT;

__u32 VERSION = SMRSIM_VERSION(1, 0, 0);

struct smrsim_c
{
   struct dm_dev *dev;   /* block_device     */
   sector_t       start; /* starting address */
};

struct mutex                      smrsim_zone_lock;
struct mutex                      smrsim_ioct_lock;
static struct smrsim_state       *zone_state = NULL;
static struct smrsim_zone_status *zone_status= NULL;

/* 
 * debug error in simulate development stage - errors should be 
 * written to log page with sense codes in actual device.
 */
static __u32 smrsim_dbg_rerr;
static __u32 smrsim_dbg_werr;
static __u32 smrsim_dbg_log_enabled = 0;
static unsigned long smrsim_dev_idle_checkpoint = 0;

/*
 * No multiple device support currently
 */
int smrsim_single = 0;

/*
 * undef the following if it is not for research WP specific op
 */
#define SMRSIM_WP_RT

/*
 * Research exercises and simulations
 *
 * The capabilities for user to configure contiguous SMR zones into
 * a bigger logical zone. By using combinational options, logically
 * different zone size can be configured. Such functions are for 
 * research and test exercises on various purposes.
 */
#ifdef SMRSIM_WP_RT

enum smrsim_rt_bodr {
   SMR_BODR_CROSS_OFF  = 0x00, /* clear       */
   SMR_BODR_CROSS_SEQ  = 0x02, /* all seq on  */
   SMR_BODR_CROSS_CUR  = 0x04  /* current on  */
};

static __u8  smrsim_wp_reset_flag  = 0;
static __u8  smrsim_wp_adjust_flag = 0;
static __u32 smrsim_wp_reset_cnt   = 0;
static __u32 smrsim_wp_adjust_cnt  = 0;

#endif

enum smrsim_conf_change {
   SMR_NO_CHANGE     = 0x00,
   SMR_CONFIG_CHANGE = 0x01,
   SMR_STATS_CHANGE  = 0x02,
   SMR_STATUS_CHANGE = 0x04
};

#define SMR_PSTORE_PG_EDG  92
#define SMR_PSTORE_PG_OFF  40
#define SMR_PSTORE_CHECK   1000
#define SMR_PSTORE_QDEPTH  128
#define SMR_PSTORE_PG_GAP  2
static struct smrsim_pstore_task {
   struct task_struct    *pstore_thread; 
   struct completion      read_event;
   struct completion      write_event;
   __u32                  sts_zone_idx;
   __u32                  stu_zone_idx[SMR_PSTORE_QDEPTH];
   __u8                   stu_zone_idx_cnt;
   __u8                   stu_zone_idx_gap;
   sector_t               pstore_lba; 
   unsigned char          flag;
} smrsim_ptask;

static __u32 smrsim_stats_size(void)
{
   return (sizeof(struct smrsim_dev_stats) + sizeof(__u32) +
           sizeof(struct smrsim_zone_stats) * SMR_NUMZONES);
}

static __u32 smrsim_state_size(void)
{
   return (sizeof(struct smrsim_state_header) +
           sizeof(struct smrsim_config) +
           sizeof(struct smrsim_dev_stats) + sizeof(__u32) +
           SMR_NUMZONES * sizeof(struct smrsim_zone_stats) +
           SMR_NUMZONES * sizeof(struct smrsim_zone_status) +
           sizeof(__u32));
}

static __u32 num_sectors_zone(void)
{
   return (1 << SMR_BLOCK_SIZE_SHIFT << SMR_ZONE_SIZE_SHIFT);
}

static __u64 zone_idx_lba(__u64 idx)
{
   return (idx << SMR_BLOCK_SIZE_SHIFT << SMR_ZONE_SIZE_SHIFT);
}

static __u64 index_power_of_2(__u64 num)
{
   __u64 index = 0;
   while (num >>= 1) {
       index++;
   }
   return index;
}

static void smrsim_dev_idle_init(void)
{
   trace_smrsim_gen_evt("dm-smrsim", "idle initialization");
   smrsim_dev_idle_checkpoint = jiffies;
   zone_state->stats.dev_stats.idle_stats.dev_idle_time_max = 0;
   zone_state->stats.dev_stats.idle_stats.dev_idle_time_min = jiffies / HZ;
}

static void smrsim_init_zone_default(__u64 sizedev)
{
   trace_smrsim_gen_evt("dm-smrsim", "zone default initialization");
   SMR_CAPACITY = sizedev;
   SMR_ZONE_SIZE_SHIFT = SMR_ZONE_SIZE_SHIFT_DEFAULT;
   SMR_BLOCK_SIZE_SHIFT = SMR_BLOCK_SIZE_SHIFT_DEFAULT;
   SMR_NUMZONES = (SMR_CAPACITY >> SMR_BLOCK_SIZE_SHIFT) 
                                >> SMR_ZONE_SIZE_SHIFT;
   SMR_NUMZONES_DEFAULT = SMR_NUMZONES;
   printk(KERN_INFO "smrsim_init_zone_state: numzones=%d sizedev=%llu\n",
      SMR_NUMZONES, sizedev);
} 

static void smrsim_init_zone_status(void)
{
   __u32 i;

   for(i=0; i< SMR_NUMZONES; i++)
   {
      zone_status[i].z_start = i;
      zone_status[i].z_length = num_sectors_zone();
      zone_status[i].z_write_ptr_offset = 0;
      zone_status[i].z_checkpoint_offset = 0;
      zone_status[i].z_type  =
         (SMR_NUMZONES > 1) && (!i || (SMR_NUMZONES-1) == i) ?
         Z_TYPE_CONVENTIONAL : Z_TYPE_SEQUENTIAL;
      zone_status[i].z_conds = 
         (SMR_NUMZONES > 1) && (!i || (SMR_NUMZONES-1) == i) ?
         Z_COND_NO_WP : Z_COND_EMPTY;
      zone_status[i].z_flag = 0;
   }
}

static void smrsim_init_zone_state_default(__u32 state_size)
{
   __u32 *magic;

   zone_state->header.magic = 0xBEEFBEEF;
   zone_state->header.length = state_size;
   zone_state->header.version = VERSION;
   zone_state->header.crc32 = 0;
   zone_state->config.dev_config.out_of_policy_read_flag  = 0;
   zone_state->config.dev_config.out_of_policy_write_flag = 0;
   zone_state->config.dev_config.r_time_to_rmw_zone = 
                                 SMR_OUT_OF_POLICY_PENALTY;
   zone_state->config.dev_config.w_time_to_rmw_zone = 
                                 SMR_OUT_OF_POLICY_PENALTY;
   zone_state->stats.num_zones = SMR_NUMZONES;
   smrsim_reset_stats();
   zone_status =(struct smrsim_zone_status *)
                 &zone_state->stats.zone_stats[SMR_NUMZONES];  
   smrsim_init_zone_status();
   magic = (__u32 *)&zone_status[SMR_NUMZONES]; 
   *magic = 0xBEEFBEEF;
}

int smrsim_init_zone_state(__u64 sizedev)
{
   __u32 state_size;

   if (!sizedev) {
      printk(KERN_ERR "smrsim: zero capacity detected\n");
      return -EINVAL;
   }
   smrsim_init_zone_default(sizedev);
   if (zone_state) {
      vfree(zone_state);
   }
   state_size = smrsim_state_size();
   zone_state = vzalloc(state_size); 
   if (!zone_state) {
      printk(KERN_ERR "smrsim: memory alloc failed for zone state\n");
      return -ENOMEM;
   }
   smrsim_init_zone_state_default(state_size);
   smrsim_dev_idle_init();
   trace_smrsim_gen_evt("dm-smrsim", "zone initialized");
   return 0;
}

static void smrsim_read_complete(struct bio *bio,
                                 int err)
{
   if (err) {
      printk(KERN_ERR "smrsim: bio read err: %d\n", err);
   }
   if (bio) {
      complete((struct completion *)bio->bi_private);
   }
}

static void smrsim_write_complete(struct bio *bio,
                                  int err)
{
   if (err) {
      printk(KERN_ERR "smrsim: bio write err: %d\n", err);
   }
   if (bio) {
      complete((struct completion *)bio->bi_private);
   }

}

static int smrsim_read_page(struct block_device *dev, 
                            sector_t lba, 
                            int size, 
                            struct page *page)
{
    int ret = 0;
    struct bio *bio = bio_alloc(GFP_NOIO, 1);

    if (!bio) {
       printk(KERN_ERR "smrsim: %s bio_alloc failed\n", __FUNCTION__);
       return -EFAULT; 
    }
    bio->bi_bdev = dev;
    #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
    bio->bi_sector = lba;
    #else
    bio->bi_iter.bi_sector = lba;
    #endif
    bio_add_page(bio, page, size, 0);
    bio->bi_private = &smrsim_ptask.read_event;
    bio->bi_end_io = smrsim_read_complete;
    submit_bio(READ | REQ_SYNC, bio);
    wait_for_completion(&smrsim_ptask.read_event);
    ret = test_bit(BIO_UPTODATE, &bio->bi_flags);
    if (!ret) {
       printk(KERN_ERR "smrsim: pstore bio read failed");
       ret = -EIO;
    }
    bio_put(bio);
    return ret;
}

static int smrsim_write_page(struct block_device *dev,
                             sector_t lba, 
                             __u32 size, 
                             struct page *page)
{
    int ret = 0;
    struct bio *bio = bio_alloc(GFP_NOIO, 1);

    if (!bio) {
       printk(KERN_ERR "smrsim: %s bio_alloc failed\n", __FUNCTION__);
       return -EFAULT; 
    }
    bio->bi_bdev = dev;
    #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
    bio->bi_sector = lba;
    #else
    bio->bi_iter.bi_sector = lba;
    #endif
    bio_add_page(bio, page, size, 0);
    bio->bi_private = &smrsim_ptask.write_event;
    bio->bi_end_io = smrsim_write_complete;
    submit_bio(WRITE_FLUSH_FUA, bio);
    wait_for_completion(&smrsim_ptask.write_event);
    ret = test_bit(BIO_UPTODATE, &bio->bi_flags);
    if (!ret) {
       printk(KERN_ERR "smrsim: pstore bio write failed");
       ret = -EIO;
    }
    bio_put(bio);
    return ret;
}

static __u32 smrsim_pstore_pg_idx(__u32 idx, 
                                  __u32 *pg_nxt)
{
   __u32 tmp = SMR_PSTORE_PG_OFF + sizeof(struct smrsim_zone_stats) * idx;
   __u32 pg_cur = tmp / PAGE_SIZE;
   
   *pg_nxt = tmp % PAGE_SIZE ? pg_cur + 1 : pg_cur;
   return pg_cur;
}

static int smrsim_flush_persistence(struct dm_target* ti)
{
   void            *page_addr;
   struct page     *page;
   struct smrsim_c *zdev;
   __u32            idx;
   __u32            crc;
   __u32            pg_cur;
   __u32            pg_nxt;
   __u32            qidx;

   zdev = ti->private;
   page = alloc_pages(GFP_KERNEL, 0);
   if (!page) {
      printk(KERN_ERR "smrsim: no enough memory to allocate a page\n");
      return -ENOMEM;
   }
   page_addr = page_address(page);
   if (!page_addr) {
      printk(KERN_ERR "smrsim: write page vm addr null\n");
      __free_pages(page, 0);
      return -EINVAL;
   }
 
   crc = crc32(0, (unsigned char *)zone_state + sizeof(struct smrsim_state_header), 
               zone_state->header.length - sizeof(struct smrsim_state_header));
   zone_state->header.crc32 = crc;
   if (smrsim_ptask.flag &= SMR_CONFIG_CHANGE) {
       smrsim_ptask.flag &= ~SMR_CONFIG_CHANGE;
   }
   memcpy(page_addr, (unsigned char *)zone_state, PAGE_SIZE);      
   smrsim_write_page(zdev->dev->bdev, smrsim_ptask.pstore_lba, PAGE_SIZE, page);
 
   if (smrsim_ptask.flag &= SMR_STATS_CHANGE) {
      smrsim_ptask.flag &= ~SMR_STATS_CHANGE;
      if (smrsim_ptask.sts_zone_idx > SMR_PSTORE_PG_EDG) {
         pg_cur = smrsim_pstore_pg_idx(smrsim_ptask.sts_zone_idx, &pg_nxt);
         smrsim_ptask.sts_zone_idx = 0; 
         for (idx = pg_cur; idx <= pg_nxt; idx++) {
            memcpy(page_addr, ((unsigned char *)zone_state + 
                   idx * PAGE_SIZE), PAGE_SIZE);
            smrsim_write_page(zdev->dev->bdev, smrsim_ptask.pstore_lba + 
                             (idx << SMR_PAGE_SIZE_SHIFT_DEFAULT), 
                             PAGE_SIZE, page);
         }
      }
   }
   if (smrsim_ptask.flag &= SMR_STATUS_CHANGE) {
      smrsim_ptask.flag &= ~SMR_STATUS_CHANGE;
      for (qidx = 0; qidx < smrsim_ptask.stu_zone_idx_cnt; qidx++) {
         pg_cur = smrsim_pstore_pg_idx(smrsim_ptask.stu_zone_idx[qidx], &pg_nxt);
         smrsim_ptask.stu_zone_idx[qidx] = 0;
         for (idx = pg_cur; idx <= pg_nxt; idx++) {
            memcpy(page_addr, ((unsigned char *)zone_state + 
                   idx * PAGE_SIZE), PAGE_SIZE);
            smrsim_write_page(zdev->dev->bdev, smrsim_ptask.pstore_lba + 
                             (idx << SMR_PAGE_SIZE_SHIFT_DEFAULT), 
                             PAGE_SIZE, page);
         }
      }
      smrsim_ptask.stu_zone_idx_cnt = 0;
      smrsim_ptask.stu_zone_idx_gap = 0;
   }
   if (smrsim_dbg_log_enabled && printk_ratelimit()) {
      printk(KERN_INFO "smrsim: flush persist success\n");
   }
   __free_pages(page, 0);
   return 0;
}

static int smrsim_save_persistence(struct dm_target* ti)
{
   void            *page_addr;
   struct page     *page;
   struct smrsim_c *zdev;
   __u32            num_pages;
   __u32            part_page;
   __u32            idx;
   __u32            crc;

   zdev = ti->private;
   page = alloc_pages(GFP_KERNEL, 0);
   if (!page) {
      printk(KERN_ERR "smrsim: no enough memory to allocate a page\n");
      return -ENOMEM;
   }
   page_addr = page_address(page);
   if (!page_addr) {
      printk(KERN_ERR "smrsim: write page vm addr null\n");
      __free_pages(page, 0);
      return -EINVAL;
   }
   num_pages = div_u64_rem(zone_state->header.length, PAGE_SIZE, &part_page);
   crc = crc32(0, (unsigned char *)zone_state + sizeof(struct smrsim_state_header), 
               zone_state->header.length - sizeof(struct smrsim_state_header));
   zone_state->header.crc32 = crc;
   for (idx = 0; idx < num_pages; idx++) {
      memcpy(page_addr, ((unsigned char *)zone_state + 
             idx * PAGE_SIZE), PAGE_SIZE);
      smrsim_write_page(zdev->dev->bdev, smrsim_ptask.pstore_lba + 
                       (idx << SMR_PAGE_SIZE_SHIFT_DEFAULT), 
                       PAGE_SIZE, page);
   }
   if (part_page) {
      memcpy(page_addr, ((unsigned char *)zone_state +
             num_pages * PAGE_SIZE), part_page);      
      smrsim_write_page(zdev->dev->bdev, smrsim_ptask.pstore_lba +
                       (num_pages << SMR_PAGE_SIZE_SHIFT_DEFAULT), 
                       PAGE_SIZE, page);
   }
   if (smrsim_dbg_log_enabled && printk_ratelimit()) {
      printk(KERN_INFO "smrsim: save persist success\n");
   }
   __free_pages(page, 0);
   return 0;
}

static int smrsim_load_persistence(struct dm_target* ti)
{
   __u64            sizedev;
   void            *page_addr;
   struct page     *page;
   struct smrsim_c *zdev;
   __u32            num_pages;
   __u32            part_page;
   __u32            idx;
   __u32            crc;
   struct smrsim_state_header header;

   printk(KERN_INFO "smrsim: Load persistence\n");
   zdev = ti->private;
   sizedev = ti->len;
   smrsim_init_zone_default(sizedev);
   smrsim_ptask.pstore_lba = SMR_NUMZONES_DEFAULT
                          << SMR_ZONE_SIZE_SHIFT_DEFAULT
                          << SMR_BLOCK_SIZE_SHIFT_DEFAULT;
   page = alloc_pages(GFP_KERNEL, 0);
   if (!page) {
      printk(KERN_ERR "smrsim: no enough memory to allocate a page\n");
      goto pgerr;
   }
   page_addr = page_address(page);
     if (!page_addr) {
      printk(KERN_ERR "smrsim: read page vm addr null\n");
      goto rderr;
   }   
   memset(page_addr, 0, PAGE_SIZE);
   smrsim_read_page(zdev->dev->bdev, smrsim_ptask.pstore_lba, PAGE_SIZE, page);
   memcpy(&header, page_addr, sizeof(struct smrsim_state_header));
   if (header.magic == 0xBEEFBEEF) {
      zone_state = vzalloc(header.length);
      if (!zone_state) {
         printk(KERN_ERR "smrsim: zome_state error: no enough memory\n");
         goto rderr;
      }
      num_pages = div_u64_rem(header.length, PAGE_SIZE, &part_page);
      if (num_pages) {
         memcpy((unsigned char *)zone_state, page_addr, PAGE_SIZE);         
      }
      for (idx = 1; idx < num_pages; idx++) {
         memset(page_addr, 0, PAGE_SIZE);
         smrsim_read_page(zdev->dev->bdev, smrsim_ptask.pstore_lba + 
            (idx << SMR_PAGE_SIZE_SHIFT_DEFAULT),
            PAGE_SIZE, page);
         memcpy(((unsigned char *)zone_state + 
            idx * PAGE_SIZE), page_addr, PAGE_SIZE);
      }
      if (part_page) {
         if (num_pages) {
            memset(page_addr, 0, PAGE_SIZE);
            smrsim_read_page(zdev->dev->bdev, smrsim_ptask.pstore_lba +
               (num_pages << SMR_PAGE_SIZE_SHIFT_DEFAULT),
                PAGE_SIZE, page);
         }
         memcpy(((unsigned char *)zone_state +
            num_pages * PAGE_SIZE), page_addr, part_page);
      }
      crc = crc32(0, (unsigned char *)zone_state + sizeof(struct smrsim_state_header), 
               zone_state->header.length - sizeof(struct smrsim_state_header));
      if (crc != zone_state->header.crc32) {
         printk(KERN_ERR "smrsim:error: crc checking. apply default config ...\n");  
         goto rderr;
      }
      SMR_NUMZONES = zone_state->stats.num_zones;
      zone_status =(struct smrsim_zone_status *)
                   &zone_state->stats.zone_stats[SMR_NUMZONES];  
      SMR_ZONE_SIZE_SHIFT = index_power_of_2(zone_status[0].z_length
		                             >> SMR_BLOCK_SIZE_SHIFT);
      printk(KERN_INFO "smrsim: Load persist success\n");
   } else {
      printk(KERN_ERR "smrsim: Load persistence magic doesn't match. Setup the default\n");
      goto rderr;
   }
   __free_pages(page, 0);
   return 0;
   rderr:
      __free_pages(page, 0);
   pgerr: 
      smrsim_init_zone_state(sizedev);
   return -EINVAL;
}

static int smrsim_persistence_task(void *arg)
{   
   struct dm_target* ti = (struct dm_target *)arg;

   while (!kthread_should_stop()) {
      if (smrsim_ptask.flag) {
         mutex_lock(&smrsim_zone_lock);
         if (smrsim_ptask.flag & SMR_CONFIG_CHANGE) {
            if (SMR_NUMZONES == 0) {
               smrsim_ptask.flag &= SMR_NO_CHANGE;
            } else {
               smrsim_save_persistence(ti);
               smrsim_ptask.flag &= SMR_NO_CHANGE;
            }
         } else {
            if (smrsim_ptask.stu_zone_idx_gap >= SMR_PSTORE_PG_GAP) {
               smrsim_save_persistence(ti);
               smrsim_ptask.flag &= SMR_NO_CHANGE;
               smrsim_ptask.stu_zone_idx_gap = 0;
               memset( smrsim_ptask.stu_zone_idx, 0, sizeof(__u32) * SMR_PSTORE_QDEPTH);
               smrsim_ptask.stu_zone_idx_cnt = 0;
            } else {
               smrsim_flush_persistence(ti);
            }
         }
         mutex_unlock(&smrsim_zone_lock);
      }
      msleep_interruptible(SMR_PSTORE_CHECK);
   }
   return 0;
}

static int smrsim_persistence_thread( struct dm_target* ti)
{
   int ret = 0;

   if (!ti) {
      printk(KERN_ERR "smrsim:warning:null device target. Improper usage\n");
      return -EINVAL;
   }
   init_completion(&smrsim_ptask.read_event);
   init_completion(&smrsim_ptask.write_event);
   smrsim_ptask.flag = 0;
   smrsim_ptask.stu_zone_idx_cnt = 0;
   smrsim_ptask.stu_zone_idx_gap = 0;
   memset( smrsim_ptask.stu_zone_idx, 0, sizeof(__u32) * SMR_PSTORE_QDEPTH);
   ret = smrsim_load_persistence(ti);
   if (ret) {
      smrsim_save_persistence(ti);
   }
   smrsim_ptask.pstore_thread = kthread_create(smrsim_persistence_task, 
                               ti, "smrsim pdthread");
   if (smrsim_ptask.pstore_thread) {
      printk(KERN_INFO "smrsim persistence thread created\n");
      wake_up_process(smrsim_ptask.pstore_thread);
   } else {
      printk(KERN_ERR "smrsim persistence thread creation failed\n");
      return -EAGAIN;
   }
   return 0;
}

static void smrsim_dev_idle_update(void)
{
   __u32 dt = 0;

   if (jiffies > smrsim_dev_idle_checkpoint) {
      dt = (jiffies - smrsim_dev_idle_checkpoint) / HZ;
   } else {
      dt = (~(__u32)0 - smrsim_dev_idle_checkpoint + jiffies) / HZ;
   } 
   if (dt > zone_state->stats.dev_stats.idle_stats.dev_idle_time_max) {
      zone_state->stats.dev_stats.idle_stats.dev_idle_time_max = dt;
   } else if (dt && (dt < zone_state->stats.dev_stats.idle_stats.dev_idle_time_min)) {
      zone_state->stats.dev_stats.idle_stats.dev_idle_time_min = dt;
   }
}

static void smrsim_report_stats(struct smrsim_stats *stats)
{
    __u32 i;
    __u32 num32 = stats->num_zones;

    if (!stats) {
       printk(KERN_ERR "smrsim: NULL pointer passed through\n");
       return;
    }
    printk("Device idle time max: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_max);
    printk("Device idle time min: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_min);
    for (i = 0; i < num32; i++) {
       printk("zone[%u] smrsim out of policy read stats: beyond swp count: %u\n",
                i, stats->zone_stats[i].out_of_policy_read_stats.beyond_swp_count);
       printk("zone[%u] smrsim out of policy read stats: span zones count: %u\n",
                i, stats->zone_stats[i].out_of_policy_read_stats.span_zones_count);
       printk("zone[%u] smrsim out of policy write stats: not on swp count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.not_on_swp_count);
       printk("zone[%u] smrsim out of policy write stats: span zones count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.span_zones_count);
       printk("zone[%u] smrsim out of policy write stats: unaligned count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.unaligned_count);
    }
}

int smrsim_get_last_rd_error(__u32 *last_error)
{
   __u32 tmperr = smrsim_dbg_rerr;

   smrsim_dbg_rerr  = 0;
   if(last_error)
      *last_error = tmperr;
   return 0;
}
EXPORT_SYMBOL(smrsim_get_last_rd_error);

int smrsim_get_last_wd_error(__u32 *last_error)
{
   __u32 tmperr = smrsim_dbg_werr;

   smrsim_dbg_werr  = 0;
   if(last_error)
      *last_error = tmperr;
   return 0;
}
EXPORT_SYMBOL(smrsim_get_last_wd_error);

int smrsim_set_log_enable(__u32 zero_is_disable)
{
   smrsim_dbg_log_enabled = zero_is_disable;
   return 0;
}
EXPORT_SYMBOL(smrsim_set_log_enable);

int smrsim_get_num_zones(__u32* num_zones)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!num_zones) {
      printk(KERN_ERR "smrsim: NULL pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   *num_zones = SMR_NUMZONES;
   mutex_unlock(&smrsim_zone_lock);
   return 0;
}
EXPORT_SYMBOL(smrsim_get_num_zones);

int smrsim_get_size_zone_default(__u32* size_zone)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!size_zone) {
      printk(KERN_ERR "smrsim: NULL pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   *size_zone = num_sectors_zone();
   mutex_unlock(&smrsim_zone_lock);
   return 0;
}
EXPORT_SYMBOL(smrsim_get_size_zone_default);

int smrsim_set_size_zone_default(__u32 size_zone)
{
   struct smrsim_state *sta_tmp;

   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if ((size_zone % (1 << SMR_BLOCK_SIZE_SHIFT)) || !(is_power_of_2(size_zone))) {
      printk(KERN_ERR "smrsim: Wong zone size specified\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   SMR_ZONE_SIZE_SHIFT = index_power_of_2((size_zone) >> SMR_BLOCK_SIZE_SHIFT);   
   SMR_NUMZONES = ((SMR_CAPACITY >> SMR_BLOCK_SIZE_SHIFT) 
                  >> SMR_ZONE_SIZE_SHIFT);
   sta_tmp = vzalloc(smrsim_state_size()); 
   if (!sta_tmp) {
      mutex_unlock(&smrsim_zone_lock);
      printk(KERN_ERR "smrsim: zone_state memory realloc failed\n");
      return -EINVAL;
   }
   vfree(zone_state);
   zone_state = sta_tmp;
   smrsim_init_zone_state_default(smrsim_state_size());
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "reset zone size to the default value");
   return 0;
}
EXPORT_SYMBOL(smrsim_set_size_zone_default);

int smrsim_reset_default_config(void)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   smrsim_reset_default_zone_config();
   smrsim_reset_default_device_config();
   trace_smrsim_gen_evt("dm-smrsim", "reset smrsim to the default config");
   return 0;
}
EXPORT_SYMBOL(smrsim_reset_default_config);

int smrsim_reset_default_device_config(void)
{
   printk(KERN_INFO "%s: called.\n", __FUNCTION__);
   mutex_lock(&smrsim_zone_lock);
   zone_state->config.dev_config.out_of_policy_read_flag  = 0;
   zone_state->config.dev_config.out_of_policy_write_flag = 0;
   zone_state->config.dev_config.r_time_to_rmw_zone = 
                                 SMR_OUT_OF_POLICY_PENALTY;
   zone_state->config.dev_config.w_time_to_rmw_zone = 
                                 SMR_OUT_OF_POLICY_PENALTY;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "reset device to the default config");
   return 0;
}
EXPORT_SYMBOL(smrsim_reset_default_device_config);

int smrsim_get_device_config(struct smrsim_dev_config *device_config)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!device_config) {
      printk(KERN_ERR "smrsim: NULL pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   memcpy(device_config, &(zone_state->config.dev_config), 
          sizeof(struct smrsim_dev_config));
   mutex_unlock(&smrsim_zone_lock);
   return 0;
}
EXPORT_SYMBOL(smrsim_get_device_config);

int smrsim_set_device_rconfig(struct smrsim_dev_config *device_config)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!device_config) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_state->config.dev_config.out_of_policy_read_flag =
      device_config->out_of_policy_read_flag;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "set device read config");
   return 0;
}
EXPORT_SYMBOL(smrsim_set_device_rconfig);

int smrsim_set_device_wconfig(struct smrsim_dev_config *device_config)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!device_config) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_state->config.dev_config.out_of_policy_write_flag =
      device_config->out_of_policy_write_flag;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "set device write config");
   return 0;
}
EXPORT_SYMBOL(smrsim_set_device_wconfig);

int smrsim_set_device_rconfig_delay(struct smrsim_dev_config *device_config)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!device_config) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   if (device_config->r_time_to_rmw_zone >= SMR_OUT_OF_POLICY_PENALTY_MAX) {
      printk(KERN_ERR "time delay exceeds default maximum\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_state->config.dev_config.r_time_to_rmw_zone =
      device_config->r_time_to_rmw_zone;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "set device read config");
   return 0;
}
EXPORT_SYMBOL(smrsim_set_device_rconfig_delay);

int smrsim_set_device_wconfig_delay(struct smrsim_dev_config *device_config)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!device_config) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   if (device_config->w_time_to_rmw_zone >= SMR_OUT_OF_POLICY_PENALTY_MAX) {
      printk(KERN_ERR "time delay exceeds allow maximum 1 minute\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_state->config.dev_config.w_time_to_rmw_zone =
      device_config->w_time_to_rmw_zone;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "set device write config");
   return 0;
}
EXPORT_SYMBOL(smrsim_set_device_wconfig_delay);

int smrsim_reset_default_zone_config(void)
{
   struct smrsim_state *sta_tmp;

   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   mutex_lock(&smrsim_zone_lock);
   SMR_NUMZONES = SMR_NUMZONES_DEFAULT;
   SMR_ZONE_SIZE_SHIFT = SMR_ZONE_SIZE_SHIFT_DEFAULT;
   sta_tmp = vzalloc(smrsim_state_size());
   vfree(zone_state); 
   if (!sta_tmp) {
      printk(KERN_ERR "smrsim: zone_state memory realloc failed\n");
      mutex_unlock(&smrsim_zone_lock);
      return -EINVAL;
   }
   zone_state = sta_tmp;
   smrsim_init_zone_state_default(smrsim_state_size());
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "reset zone to the default config");
   return 0;
}
EXPORT_SYMBOL(smrsim_reset_default_zone_config);

int smrsim_clear_zone_config(void)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   memset(zone_state->stats.zone_stats, 0, zone_state->stats.num_zones *
          sizeof (struct smrsim_zone_stats));
   mutex_lock(&smrsim_zone_lock);
   zone_state->stats.num_zones = 0;   
   memset(zone_status, 0, SMR_NUMZONES * sizeof (struct smrsim_zone_status));
   SMR_NUMZONES = 0;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "zone cleaned to empty");
   return 0;
}
EXPORT_SYMBOL(smrsim_clear_zone_config);

static int smrsim_zone_seq_count(void)
{
   __u32 count = 0;
   __u32 index;
  
   for (index = 0; index < SMR_NUMZONES; index++)
   {
      if (zone_status[index].z_type == Z_TYPE_SEQUENTIAL)
      {
          count++;
      }
   }
   return count;
}

static int smrsim_zone_cond_check(__u16 cond)
{
   switch (cond) {
      case Z_COND_NO_WP:
      case Z_COND_EMPTY:
      case Z_COND_CLOSED:
      case Z_COND_RO:
      case Z_COND_FULL:
      case Z_COND_OFFLINE:
         return 1;
      default:
         return 0;
   }
   return 0;
}

int smrsim_modify_zone_config(struct smrsim_zone_status *z_status)
{ 
   __u32 count = smrsim_zone_seq_count();

   printk(KERN_INFO "%s: called.\n", __FUNCTION__);
   if (!z_status) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   if (SMR_NUMZONES <= z_status->z_start) {
      printk(KERN_ERR "smrsim: config does not exist\n");
      return -EINVAL;
   } 
   if (1 >= count && (Z_TYPE_CONVENTIONAL == z_status->z_type) &&
       (Z_TYPE_SEQUENTIAL == zone_status[z_status->z_start].z_type)) {
      printk(KERN_ERR "smrsim: zone type is not allowed to modify\n");
      return -EINVAL;
   }
   if (z_status->z_length != num_sectors_zone()) {
      printk(KERN_ERR "smrsim: zone size is not allowed to change individually\n");
      return -EINVAL;
   }
   if (z_status->z_write_ptr_offset > num_sectors_zone()) {
      printk(KERN_ERR "smrsim: zone wp is out of range\n");
      return -EINVAL;
   }
   if ((z_status->z_write_ptr_offset == num_sectors_zone()) 
      && (zone_status[z_status->z_start].z_conds != Z_COND_FULL)) {
      printk(KERN_ERR "smrsim: zone wp and condition mismatch\n");
      return -EINVAL;
   }
   if (z_status->z_checkpoint_offset >= num_sectors_zone()) {
      printk(KERN_ERR "smrsim: zone checkpoint is out of range\n");
      return -EINVAL;
   }
   if (!smrsim_zone_cond_check(z_status->z_conds)) {
      printk(KERN_ERR "smrsim: wrong zone condition\n");
      return -EINVAL;
   }
   if ((Z_COND_NO_WP == z_status->z_conds) && 
       (Z_TYPE_CONVENTIONAL != z_status->z_type)) {
      printk(KERN_ERR "smrsim: condition and type mismatch\n");
      return -EINVAL;
   }
   if ((Z_COND_EMPTY == z_status->z_conds) && 
       (Z_TYPE_SEQUENTIAL == z_status->z_type) &&
       (0 != z_status->z_write_ptr_offset)) {
      printk(KERN_ERR "smrsim: empty zone isn't empty\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_status[z_status->z_start].z_write_ptr_offset =
      z_status->z_write_ptr_offset;   
   zone_status[z_status->z_start].z_checkpoint_offset =
      z_status->z_checkpoint_offset;   
   zone_status[z_status->z_start].z_conds = 
      (enum smrsim_zone_conditions)z_status->z_conds;
   zone_status[z_status->z_start].z_type = 
      (enum smrsim_zone_type)z_status->z_type;
   zone_status[z_status->z_start].z_flag = 0;
   mutex_unlock(&smrsim_zone_lock);
   printk(KERN_DEBUG "smrsim: zone[%lu] modified. type:0x%x conds:0x%x\n",
      zone_status[z_status->z_start].z_start,
      zone_status[z_status->z_start].z_type, 
      zone_status[z_status->z_start].z_conds);
   trace_smrsim_gen_evt("dm-smrsim", "the zone modified");
   return 0;
}
EXPORT_SYMBOL(smrsim_modify_zone_config);

int smrsim_add_zone_config(struct smrsim_zone_status *zone_sts)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!zone_sts) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   if (zone_sts->z_start >= SMR_NUMZONES_DEFAULT) {
      printk(KERN_ERR "smrsim: zone config start lba is out of range\n");
      return -EINVAL;
   }
   if (zone_sts->z_start != SMR_NUMZONES) {
      printk(KERN_ERR "smrsim: zone config does not start at the end of current zone\n");
      printk(KERN_INFO "smrsim: z_start: %u  SMR_NUMZONES: %u\n", (__u32)zone_sts->z_start,
             SMR_NUMZONES);
      return -EINVAL;
   }
   if ((zone_sts->z_type != Z_TYPE_CONVENTIONAL) && (zone_sts->z_type != Z_TYPE_SEQUENTIAL)) {
      printk(KERN_ERR "smrsim: zone config type is not allowed with current config\n");
      return -EINVAL;
   }
   if ((zone_sts->z_type == Z_TYPE_CONVENTIONAL) && (zone_sts->z_conds != Z_COND_NO_WP)) {
      printk(KERN_ERR "smrsim: zone config condition is wrong. Need to be NO WP\n");
      return -EINVAL;
   }
   if ((zone_sts->z_type == Z_TYPE_SEQUENTIAL) && (zone_sts->z_conds != Z_COND_EMPTY)) {
      printk(KERN_ERR "smrsim: zone config condition is wrong. Need to be EMPTY\n");
      return -EINVAL;
   }
   if (zone_sts->z_length != (1 << SMR_ZONE_SIZE_SHIFT << SMR_BLOCK_SIZE_SHIFT)) {
      printk(KERN_ERR "smrsim: zone config size is not allowed with current config\n");
      return -EINVAL;
   }
   if (zone_sts->z_write_ptr_offset != 0) {
      printk(KERN_ERR "smrsim: zone config write pointer isn't at the start point.\n");
      return -EINVAL;
   }
   zone_sts->z_flag = 0;
   mutex_lock(&smrsim_zone_lock);
   memcpy(&(zone_status[SMR_NUMZONES]), zone_sts, sizeof(struct smrsim_zone_status));
   zone_state->stats.num_zones++;
   SMR_NUMZONES++;
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "the zone added");
   return 0;
}
EXPORT_SYMBOL(smrsim_add_zone_config);

int smrsim_reset_zone_stats(sector_t start_sector)
{
   __u32 zone_idx  = start_sector >> SMR_BLOCK_SIZE_SHIFT >> SMR_ZONE_SIZE_SHIFT; 

   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (SMR_NUMZONES <= zone_idx) {
      printk(KERN_ERR "smrsim: %s start sector is out of range\n", __FUNCTION__);  
      return -EINVAL;
   }
   memset(&(zone_state->stats.zone_stats[zone_idx].out_of_policy_read_stats),
          0, sizeof(struct smrsim_out_of_policy_read_stats));
   memset(&(zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats),
          0, sizeof(struct smrsim_out_of_policy_write_stats));
   trace_smrsim_gen_evt("dm-smrsim", "zone stats reset");
   return 0;
}
EXPORT_SYMBOL(smrsim_reset_zone_stats);

int smrsim_reset_stats(void)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   memset(&zone_state->stats.dev_stats.idle_stats, 0, sizeof(struct smrsim_idle_stats));
   memset(zone_state->stats.zone_stats, 0, zone_state->stats.num_zones * 
          sizeof(struct smrsim_zone_stats));
   trace_smrsim_gen_evt("dm-smrsim", "reset zone stats"); 
   return 0;
}
EXPORT_SYMBOL(smrsim_reset_stats);

int smrsim_get_stats(struct smrsim_stats *stats)
{
   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   if (!stats) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   memcpy(stats, &(zone_state->stats), smrsim_stats_size());
   return 0;
}
EXPORT_SYMBOL(smrsim_get_stats);

int smrsim_blkdev_reset_zone_ptr(sector_t start_sector)
{
   __u32 rem;
   __u32 zone_idx;

   printk(KERN_INFO "smrsim: %s: called.\n", __FUNCTION__);
   mutex_lock(&smrsim_zone_lock);
   zone_idx = start_sector >> SMR_BLOCK_SIZE_SHIFT >> SMR_ZONE_SIZE_SHIFT; 
   if (SMR_NUMZONES <= zone_idx) {
      mutex_unlock(&smrsim_zone_lock);
      printk(KERN_ERR "smrsim: %s start_sector is out of range\n", __FUNCTION__);  
      return -EINVAL;
   }
   if (zone_status[zone_idx].z_type == Z_TYPE_CONVENTIONAL) {
      mutex_unlock(&smrsim_zone_lock);
      printk(KERN_ERR "smrsim:error: CMR zone dosen't have a write pointer.\n");
      return -EINVAL;
   }
   div_u64_rem(start_sector, (1 << SMR_BLOCK_SIZE_SHIFT << SMR_ZONE_SIZE_SHIFT), &rem);
   if (rem) {
      mutex_unlock(&smrsim_zone_lock);
      printk(KERN_ERR "smrsim: %s start_sector is not the begining of a zone\n", 
             __FUNCTION__);  
      return -EINVAL;
   }
   zone_status[zone_idx].z_write_ptr_offset = 0;
   if (zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) {
      zone_status[zone_idx].z_conds = Z_COND_EMPTY;
   } 
   mutex_unlock(&smrsim_zone_lock);
   trace_smrsim_gen_evt("dm-smrsim", "zone wp reset");
   return 0;
}
EXPORT_SYMBOL(smrsim_blkdev_reset_zone_ptr);

void smrsim_log_error(struct bio* bio,
                      __u32 uerr)
{
   __u64 lba;

   if (!bio) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return;
   }
   #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
      lba = bio->bi_sector;
   #else
      lba = bio->bi_iter.bi_sector;
   #endif
   if (smrsim_dbg_log_enabled) {
      switch(uerr)
      {
         case SMR_ERR_READ_BORDER:
            printk(KERN_DEBUG "%s: lba:%llu SMR_ERR_READ_BORDER\n", __FUNCTION__, lba);
            smrsim_dbg_rerr = uerr;
            break;
         case SMR_ERR_READ_POINTER: 
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_READ_POINTER\n",__FUNCTION__, lba);
            smrsim_dbg_rerr = uerr;
            break;
         case SMR_ERR_WRITE_RO:
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_WRITE_RO\n", __FUNCTION__, lba);
            smrsim_dbg_werr = uerr;
            break;
         case SMR_ERR_WRITE_POINTER :
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_WRITE_POINTER\n",__FUNCTION__, lba);
            smrsim_dbg_werr = uerr;
            break;
         case SMR_ERR_WRITE_ALIGN :
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_WRITE_ALIGN\n", __FUNCTION__, lba);
            smrsim_dbg_werr = uerr;
            break;
         case SMR_ERR_WRITE_BORDER:
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_WRITE_BORDER\n", __FUNCTION__, lba);
            smrsim_dbg_werr = uerr;
            break;
         case SMR_ERR_WRITE_FULL:
            printk(KERN_DEBUG "%s: lba:%llu: SMR_ERR_WRITE_FULL\n", __FUNCTION__, lba);
            smrsim_dbg_werr = uerr;
            break;
         default:
            printk(KERN_DEBUG "%s: lba:%llu: UNKNOWN ERR=%u\n", __FUNCTION__, lba, uerr);
      }
   }
}

static int smrsim_ctr(struct dm_target* ti, 
                      unsigned int argc,
                      char** argv)
{
   unsigned long long tmp;
   int iRet;
   char dummy;
   struct smrsim_c* c = NULL;
   __u64 num;
   
   printk(KERN_INFO "%s called\n", __FUNCTION__);
   if (smrsim_single) {
      printk(KERN_ERR "No multiple device support currently\n");
      return -EINVAL;
   }
   if (!ti) {
      printk(KERN_ERR "smrsim:error: invalid device\n");
      return -EINVAL;
   }
   if (2 != argc) {
      ti->error = "dm-smrsim:error: invalid argument count; !=2";
      return -EINVAL;
   }
   if (1 != sscanf(argv[1], "%llu%c", &tmp, &dummy)) {
      ti->error = "dm-smrsim:error: invalid argument device sector";
      return -EINVAL;
   }
   trace_smrsim_ctr_evt(argv[0], tmp, "start");
   c = kmalloc(sizeof(*c), GFP_KERNEL);
   if (!c) {
      ti->error = "dm-smrsim:error: no enough memory";
      return -ENOMEM;
   }
   c->start = tmp;
   iRet = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &c->dev);
   if (iRet) {
      ti->error = "dm-smrsim:error: device lookup failed";
      kfree(c);
      return iRet;
   }
   if (ti->len > SMR_MAX_CAPACITY) {
      printk(KERN_ERR "smrsim:error: capacity %llu exceeds the maximum 10TB\n",
            (__u64)ti->len);
      kfree(c);
      return -EINVAL;
   }
   num = ti->len >> SMR_BLOCK_SIZE_SHIFT >> SMR_ZONE_SIZE_SHIFT;
   if ((num << SMR_BLOCK_SIZE_SHIFT << SMR_ZONE_SIZE_SHIFT) != ti->len) {
      printk(KERN_ERR "smrsim:error: total size must be zone size (256MB) aligned\n");
   }
   if (ti->len < (1 << SMR_BLOCK_SIZE_SHIFT << SMR_ZONE_SIZE_SHIFT)) {
      printk(KERN_INFO "smrsim: capacity: %llu sectors\n", (__u64)ti->len);
      printk(KERN_ERR "smrsim:error: capacity is too small. The default config is multiple of 256MB\n"); 
      kfree(c);
      return -EINVAL;
   }
   ti->num_flush_bios = ti->num_discard_bios = ti->num_write_same_bios = 1;
   ti->private = c;
   smrsim_dbg_rerr = 0;
   smrsim_dbg_werr = 0;
   smrsim_dbg_log_enabled = 0;
   mutex_init(&smrsim_zone_lock);
   mutex_init(&smrsim_ioct_lock);
   if (smrsim_persistence_thread(ti)) {
      printk(KERN_ERR "smrsim:error: metadata will not be persisted\n");
   }
   smrsim_single = 1;
   return 0;
}

static void smrsim_dtr(struct dm_target *ti)
{
   struct smrsim_c *c = (struct smrsim_c*) ti->private;

   kthread_stop(smrsim_ptask.pstore_thread);
   mutex_destroy(&smrsim_zone_lock);
   mutex_destroy(&smrsim_ioct_lock);
   dm_put_device(ti, c->dev);
   kfree(c);
   vfree(zone_state);
   smrsim_single = 0;
   printk(KERN_INFO "smrsim target destructed\n");
   trace_smrsim_gen_evt("dm-smrsim", "smrsim target destructed");
}

static sector_t smrsim_map_sector(struct dm_target *ti,
                                  sector_t bi_sector)
{
   struct smrsim_c *c = ti->private;
   return c->start + dm_target_offset(ti, bi_sector);
}

int smrsim_write_rule_check(struct bio *bio,
                            __u32 zone_idx, 
                            sector_t bio_sectors,
                            int policy_flag)
{
   __u64 lba;
   __u64 size; 
   __u64 elba;
   __u32 rem;
   __u64 zlba;
   __u32 rv;
   __u32 z_size;
   __u32 idx;
   __u32 eidx;

   #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
      lba  = bio->bi_sector;
      size = bio->bi_size;
   #else
      lba  = bio->bi_iter.bi_sector;
      size =  bio->bi_iter.bi_size;
   #endif
   rv = 0;
   elba   = lba + bio_sectors;
   if (zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) {
      div_u64_rem(size, 4096, &rem);
      if (rem) {
         printk(KERN_ERR "smrsim:error: %s size is not 4k aligned. zone_idx: %u\n", 
            __FUNCTION__, zone_idx); 
         zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.unaligned_count++;
         smrsim_log_error(bio, SMR_ERR_WRITE_ALIGN);
         rv++;
         if (!policy_flag) {
            return SMR_ERR_WRITE_ALIGN;
         }
      }
   }
   z_size = num_sectors_zone();  
   zlba = zone_idx_lba(zone_idx);
   if ((zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) &&
      (zlba + zone_status[zone_idx].z_write_ptr_offset != lba)) {
      #ifdef SMRSIM_WP_RT
      if (smrsim_wp_reset_flag && (lba == zlba)) {
         smrsim_wp_reset_cnt++;
         printk(KERN_ERR "smrsim:error: rt reset pass: %s zone_idx.counter: %u.%u\n", 
            __FUNCTION__, zone_idx, smrsim_wp_reset_cnt);
         zone_status[zone_idx].z_write_ptr_offset = 0;
         goto hcerr;
      } 
      else if (smrsim_wp_adjust_flag && (lba > (zlba + 
         zone_status[zone_idx].z_write_ptr_offset))) {
         smrsim_wp_adjust_cnt++;
         zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.not_on_swp_count++;
         printk(KERN_ERR "smrsim:error: rt write ahead pass: zone_idx.counter: %u.%u\n",
            zone_idx, smrsim_wp_adjust_cnt);
         zone_status[zone_idx].z_write_ptr_offset = lba - zlba;
         goto hcerr; 
      }
      #endif
      printk(KERN_ERR "smrsim:error: %s write isn't at wp: %u.%012llx.%08lx wp: %08x\n",
         __FUNCTION__, zone_idx, lba, bio_sectors, zone_status[zone_idx].z_write_ptr_offset);
      zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.not_on_swp_count++;
      smrsim_log_error(bio, SMR_ERR_WRITE_POINTER);
      if (!policy_flag) {
         rv++;
         return SMR_ERR_WRITE_POINTER;
      }
      #ifdef SMRSIM_WP_RT
      hcerr:
      #endif
      rv++;
   }
   /* internal trace */
   if (zone_status[zone_idx].z_type == Z_TYPE_CONVENTIONAL) {
      if (elba == (zone_idx_lba(zone_idx) + z_size)) { 
         zone_status[zone_idx].z_write_ptr_offset = z_size;
         if (smrsim_dbg_log_enabled && printk_ratelimit()) {
            printk(KERN_DEBUG "smrsim: conventional zone fill up a zone\n");
            printk(KERN_DEBUG "smrsim: %s %u.%012llx.%08lx\n",
               __FUNCTION__, zone_idx, lba, bio_sectors);
         }
         return 0;
      } else {
         if ((elba > (zone_idx_lba(zone_idx) + zone_status[zone_idx].z_write_ptr_offset)) &&
             (elba < (zone_idx_lba(zone_idx) + z_size))) {
            zone_status[zone_idx].z_write_ptr_offset = elba - zone_idx_lba(zone_idx);
            if (smrsim_dbg_log_enabled && printk_ratelimit()) {
               printk(KERN_DEBUG "smrsim: conventional zone write ahead\n");
               printk(KERN_DEBUG "smrsim: %s %u.%012llx.%08lx\n",
                  __FUNCTION__, zone_idx, lba, bio_sectors);
            }
            return 0;
         } else if (elba < (zone_idx_lba(zone_idx) + zone_status[zone_idx].z_write_ptr_offset)){
            if (smrsim_dbg_log_enabled && printk_ratelimit()) {
               printk(KERN_DEBUG "smrsim: conventional zone modify\n"); 
               printk(KERN_DEBUG "smrsim: %s %u.%012llx.%08lx\n",
                  __FUNCTION__, zone_idx, lba, bio_sectors);
            }
            return 0;
         }
      }
   }
   if ((elba > (zlba + z_size))) {
      #ifdef SMRSIM_WP_RT
      if (elba <= (zlba + 2 * z_size)) {
         if ((zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) && 
            (zone_status[zone_idx + 1].z_type == Z_TYPE_SEQUENTIAL) && 
            ((zone_idx + 1) < SMR_NUMZONES) && 
            ((zone_status[zone_idx].z_flag == SMR_BODR_CROSS_SEQ) || 
            (zone_status[zone_idx].z_flag == SMR_BODR_CROSS_CUR)) &&
            ((smrsim_wp_reset_flag == 1) || (zone_status[zone_idx + 1].z_write_ptr_offset == 0))) {
            printk(KERN_ERR "smrsim:error: research split: %u.%012llx.%08lx type: 0x%x\n",
               zone_idx, lba, bio_sectors, zone_status[zone_idx].z_type);
            zone_status[zone_idx].z_conds = Z_COND_FULL; 
            zone_status[zone_idx].z_write_ptr_offset = z_size;
            zone_status[zone_idx + 1].z_write_ptr_offset = elba - zlba - z_size;
            zone_status[zone_idx + 1].z_conds = Z_COND_CLOSED;
            zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.span_zones_count++;
            rv++;
            return 0;
         }
      } else {
         printk(KERN_ERR "smrsim:error: write across border more than one zone\n");
      } 
      #endif
      if (zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) {
         printk(KERN_ERR "smrsim:error: write acrossed border: %u.%012llx.%08lx type: 0x%x\n",
            zone_idx, lba, bio_sectors, zone_status[zone_idx].z_type);
         zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.span_zones_count++;
         smrsim_log_error(bio, SMR_ERR_WRITE_BORDER);
         rv++;
         if (!policy_flag) {
            return SMR_ERR_WRITE_BORDER;
         }
      }
      eidx = elba >> SMR_BLOCK_SIZE_SHIFT >> SMR_ZONE_SIZE_SHIFT;
      if (zone_status[zone_idx].z_type == Z_TYPE_CONVENTIONAL) {
         for (idx = zone_idx + 1; idx <= eidx; idx++) {
            if (zone_status[idx].z_type != Z_TYPE_CONVENTIONAL) {
               zone_state->stats.zone_stats[zone_idx].out_of_policy_write_stats.span_zones_count++;
               printk(KERN_ERR "smrsim:error: write across CMR zone to SMR zone\n");
               if (!policy_flag) {
                  return SMR_ERR_WRITE_BORDER;
               } else {
                   printk(KERN_ERR "smrsim:error: rt allowed write across CMR to SMR zone\n");
                   break;
               }
            }
         }
      }
      if ((zone_status[zone_idx].z_type == Z_TYPE_CONVENTIONAL) || (policy_flag == 1)) {
         for (idx = zone_idx; idx < eidx; idx++) {
            zone_status[idx].z_write_ptr_offset = z_size;
            if (zone_status[idx].z_type == Z_TYPE_CONVENTIONAL) {
               zone_status[idx].z_conds = Z_COND_NO_WP; 
            } else {
               zone_status[idx].z_conds = Z_COND_FULL;
            } 
         }
         zone_status[eidx].z_write_ptr_offset = (elba - zlba - z_size) % z_size;
         if (zone_status[eidx].z_type == Z_TYPE_SEQUENTIAL) {
            if (zone_status[eidx].z_write_ptr_offset != z_size) {
               zone_status[eidx].z_conds = Z_COND_CLOSED;
            } else {
               zone_status[eidx].z_conds = Z_COND_FULL;
            }
         }
         if (policy_flag == 1) {
            return SMR_ERR_OUT_OF_POLICY;
         }
         return 0;
      }
   }
   if ((policy_flag == 1) && (zone_status[zone_idx].z_conds == Z_COND_FULL)) {
      zone_status[zone_idx].z_write_ptr_offset = elba - zlba;
      if (zone_status[zone_idx].z_write_ptr_offset == z_size) {
         zone_status[zone_idx].z_conds = Z_COND_FULL; 
      } else {
         zone_status[zone_idx].z_conds = Z_COND_CLOSED; 
      }      
   } else { 
      trace_smrsim_zone_write_evt(zone_idx, zone_status[zone_idx].z_write_ptr_offset,
         zone_status[zone_idx].z_write_ptr_offset + bio_sectors);

      zone_status[zone_idx].z_write_ptr_offset =  
         zone_status[zone_idx].z_write_ptr_offset + bio_sectors;
      if (zone_status[zone_idx].z_type == Z_TYPE_SEQUENTIAL) {
         if (zone_status[zone_idx].z_write_ptr_offset == z_size) {
            zone_status[zone_idx].z_conds = Z_COND_FULL; 
         } else {
            zone_status[zone_idx].z_conds = Z_COND_CLOSED;
         } 
      }
   }
   if (smrsim_dbg_log_enabled && printk_ratelimit()) {

      printk(KERN_INFO "smrsim write PASS\n");
   }
   if (rv && (policy_flag ==1)) {
      printk(KERN_ERR "smrsim: out of policy passed rule violation: %u\n", rv); 
      return SMR_ERR_OUT_OF_POLICY;
   }
   return 0;
}

int smrsim_read_rule_check(struct bio *bio,
                            __u32 zone_idx, 
                            sector_t bio_sectors,
                            int policy_flag)
{
   __u64 lba;
   __u64 zlba;
   __u64 elba;
   __u32 rv;

   #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
      lba = bio->bi_sector;
   #else
      lba = bio->bi_iter.bi_sector;
   #endif
   rv = 0;
   elba   = lba + bio_sectors;
   zlba = zone_idx_lba(zone_idx);
   if (elba > (zlba + num_sectors_zone())) {
      printk(KERN_ERR "smrsim:error: read across zone: %u.%012llx.%08lx\n",
         zone_idx, lba, bio_sectors);
      rv++;
      zone_state->stats.zone_stats[zone_idx].out_of_policy_read_stats.span_zones_count++;
      smrsim_log_error(bio, SMR_ERR_READ_BORDER);
      if (!policy_flag) {
         return SMR_ERR_READ_BORDER;
      }
      printk(KERN_ERR "smrsim:error: out of policy allowed pass\n");
   }

   if (zone_status[zone_idx].z_type == Z_TYPE_CONVENTIONAL) {
      if (smrsim_dbg_log_enabled && printk_ratelimit()) {
         printk(KERN_INFO "smrsim: conventional zone skip wp check\n");
      }
      goto next;  
   }
   trace_smrsim_zone_read_evt(zone_idx, zone_status[zone_idx].z_write_ptr_offset);   

   if (elba > (zlba + zone_status[zone_idx].z_write_ptr_offset)) {
      if (printk_ratelimit()) {
         printk(KERN_ERR "smrsim:error: %s read beyond wp: %u.%012llx.%08lx wp: %08x\n",
            __FUNCTION__, zone_idx, lba, bio_sectors, zone_status[zone_idx].z_write_ptr_offset);
      }
      rv++;
      zone_state->stats.zone_stats[zone_idx].out_of_policy_read_stats.beyond_swp_count++;
      smrsim_log_error(bio, SMR_ERR_READ_POINTER);
      if (!policy_flag) {
         return SMR_ERR_READ_POINTER;
      }
      printk(KERN_ERR "smrsim:error: out of policy allowed pass\n");
   }
   next:   
   if (smrsim_dbg_log_enabled && printk_ratelimit()) {
      printk(KERN_INFO "smrsim read PASS\n");
   }
   if (rv) {
      printk(KERN_ERR "smrsim: out of policy passed rule violation: %u\n", rv); 
      return SMR_ERR_OUT_OF_POLICY;
   }
   return 0;
}

static bool smrsim_ptask_queue_ok(__u32 idx)
{
   __u32 qidx;
   
    for (qidx = 0; qidx < smrsim_ptask.stu_zone_idx_cnt; qidx++) {
       if (abs(idx - (smrsim_ptask.stu_zone_idx[qidx]))
          <= SMR_PSTORE_PG_EDG) {
          return false;
       }
    }
    return true;
}

static bool smrsim_ptask_gap_ok(__u32 idx)
{
   __u32 qidx;
   
    for (qidx = 0; qidx < smrsim_ptask.stu_zone_idx_cnt; qidx++) {
       if (abs(idx - (smrsim_ptask.stu_zone_idx[qidx]))
          <= SMR_PSTORE_PG_GAP * SMR_PSTORE_PG_EDG) {
          return true;
       }
    }
    return false;
}

int smrsim_map(struct dm_target *ti, 
               struct bio *bio)
{
   struct smrsim_c* c = ti->private;
   int cdir = bio_data_dir(bio);

   sector_t bio_sectors = bio_sectors(bio);
   int policy_rflag = 0;
   int policy_wflag = 0;
   int ret = 0;
   unsigned int penalty;
   __u32 zone_idx;
   __u64 lba;

   mutex_lock(&smrsim_zone_lock);
   #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
   zone_idx = bio->bi_sector >> SMR_BLOCK_SIZE_SHIFT 
                             >> SMR_ZONE_SIZE_SHIFT;
   lba = bio->bi_sector;
   #else
   zone_idx = bio->bi_iter.bi_sector >> SMR_BLOCK_SIZE_SHIFT 
                                     >> SMR_ZONE_SIZE_SHIFT;
   lba = bio->bi_iter.bi_sector;
   #endif

   trace_smrsim_block_io_evt("dm-smrsim", bio);
   smrsim_dev_idle_update();

   if (SMR_NUMZONES <= zone_idx) {
      printk(KERN_ERR "smrsim: lba is out of range. zone_idx: %u\n", zone_idx);
      smrsim_log_error(bio, SMR_ERR_OUT_RANGE);
      goto nomap;
   }
   if (smrsim_dbg_log_enabled) {
      printk(KERN_DEBUG "smrsim: %s bio_sectors=%llu\n", __FUNCTION__,
         (unsigned long long)bio_sectors);
   }
   if ((lba + bio_sectors) > (zone_idx_lba(zone_idx) + 2 * num_sectors_zone())) {
      printk(KERN_ERR "smrsim:error: %s bio_sectors() is too large\n", __FUNCTION__);  
      smrsim_log_error(bio, SMR_ERR_OUT_OF_POLICY);
      goto nomap;
   } 
   if (zone_status[zone_idx].z_conds == Z_COND_OFFLINE) {
      printk(KERN_ERR "smrsim:error: zone is offline. zone_idx: %u\n", zone_idx);  
      smrsim_log_error(bio, SMR_ERR_ZONE_OFFLINE);
      goto nomap;
   }
   bio->bi_bdev = c->dev->bdev;
   policy_rflag = zone_state->config.dev_config.out_of_policy_read_flag;
   policy_wflag = zone_state->config.dev_config.out_of_policy_write_flag;
   if (cdir == WRITE) {
      if (smrsim_dbg_log_enabled) {
         printk(KERN_DEBUG "smrsim: %s WRITE %u.%012llx:%08lx WP=%08x.\n", __FUNCTION__,
                zone_idx, lba, bio_sectors, zone_status[zone_idx].z_write_ptr_offset);
      }
      if ((zone_status[zone_idx].z_conds == Z_COND_RO) && !policy_wflag) {
         printk(KERN_ERR "smrsim:error: zone is read only. zone_idx: %u\n", zone_idx);  
         smrsim_log_error(bio, SMR_ERR_WRITE_RO);
         goto nomap;
      }
      if ((zone_status[zone_idx].z_conds == Z_COND_FULL) &&
          (lba != zone_idx_lba(zone_idx)) && !policy_wflag) {
         printk(KERN_ERR "smrsim:error: zone is full. zone_idx: %u\n", zone_idx);
         smrsim_log_error(bio, SMR_ERR_WRITE_FULL);
         goto nomap;
      }
      ret = smrsim_write_rule_check(bio, zone_idx, bio_sectors, policy_wflag);
      if (ret) {
         if (policy_wflag == 1 && policy_rflag ==1) {
            goto mapped;
         }
         penalty = 0;
         if (policy_wflag == 1) {
            penalty = zone_state->config.dev_config.w_time_to_rmw_zone;
            trace_smrsim_bio_oop_write_check_evt("out of policy: bio write error pass",
               policy_wflag, penalty, ret);
            printk(KERN_ERR "smrsim:%s: write error passed: out of policy write flagged on\n", 
               __FUNCTION__);
            msleep_interruptible(penalty);
         } else {
            trace_smrsim_bio_write_check_evt("write error out of policy", policy_wflag, ret);
            goto nomap;
         } 
      }
      smrsim_ptask.flag |= SMR_STATUS_CHANGE;
      if (smrsim_ptask.stu_zone_idx_cnt == SMR_PSTORE_QDEPTH) {
         smrsim_ptask.stu_zone_idx_gap = SMR_PSTORE_PG_GAP;
      } else if (smrsim_ptask_queue_ok(zone_idx)) {
         smrsim_ptask.stu_zone_idx[smrsim_ptask.stu_zone_idx_cnt] = zone_idx;
         smrsim_ptask.stu_zone_idx_cnt++;
         if (!smrsim_ptask_gap_ok(zone_idx)) {
            smrsim_ptask.stu_zone_idx_gap++;  
         }
      }
   }
   else if (cdir == READ) {
      if (smrsim_dbg_log_enabled) {
         printk(KERN_DEBUG "smrsim: %s READ %u.%012llx:%08lx WP=%08x.\n", __FUNCTION__,
                zone_idx, lba, bio_sectors, zone_status[zone_idx].z_write_ptr_offset);
      }
      ret = smrsim_read_rule_check(bio, zone_idx, bio_sectors, policy_rflag);
      if (ret) {
         if (policy_wflag == 1 && policy_rflag ==1) {
            printk(KERN_ERR "smrsim: out of policy read passthrough applied\n");
            goto mapped;
         }
         penalty = 0;
         if (policy_rflag == 1) {
            penalty = zone_state->config.dev_config.r_time_to_rmw_zone;
            trace_smrsim_bio_oop_read_check_evt("out of policy: bio read error", policy_rflag,
               penalty, ret);
            if (printk_ratelimit()) {
               printk(KERN_ERR "smrsim:%s: read error passed: out of policy read flagged on\n", 
                  __FUNCTION__);
            }
            msleep_interruptible(penalty);
         } else {
            trace_smrsim_bio_read_check_evt("read error out of policy", policy_rflag, ret);
            goto nomap;
         }
      }
   }
   mapped:
   if (bio_sectors(bio))
   #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
      bio->bi_sector =  c->start + dm_target_offset(ti, bio->bi_sector);
   #else
      bio->bi_iter.bi_sector =  c->start + dm_target_offset(ti, bio->bi_iter.bi_sector);
   #endif
   mutex_unlock(&smrsim_zone_lock);
   return DM_MAPIO_REMAPPED;
   nomap:
   smrsim_ptask.flag |= SMR_STATS_CHANGE;
   smrsim_ptask.sts_zone_idx = zone_idx;
   mutex_unlock(&smrsim_zone_lock);
   return SMR_DM_IO_ERR;  
}

static void smrsim_status(struct dm_target* ti, 
                          status_type_t type,
                          unsigned status_flags, 
                          char* result,
                          unsigned maxlen)
{
   struct smrsim_c* c   = ti->private;

   switch(type)
   {
      case STATUSTYPE_INFO:
         result[0] = '\0';
         break;

      case STATUSTYPE_TABLE:
         snprintf(result, maxlen, "%s %llu", c->dev->name,
	    (unsigned long long)c->start);
         break;
   }
}

static void smrsim_list_zone_status(struct smrsim_zone_status *ptr, 
                                    __u32 num_zones,
                                    int criteria)
{
   __u32 i = 0;
   printk(KERN_DEBUG "\nQuery ceiteria: %d\n", criteria);
   printk(KERN_DEBUG "List zone status of %u zones:\n\n", num_zones);
   for (i = 0; i < num_zones; i++) {
       printk(KERN_DEBUG "zone index        : %lu\n", (long unsigned)ptr[i].z_start);
       printk(KERN_DEBUG "zone length       : %u\n",  ptr[i].z_length);
       printk(KERN_DEBUG "write offset      : %u\n",  ptr[i].z_write_ptr_offset);
       printk(KERN_DEBUG "checkpoint offset : %u\n",  ptr[i].z_checkpoint_offset);
       printk(KERN_DEBUG "zone type         : 0x%x\n", ptr[i].z_type);
       printk(KERN_DEBUG "zone condition    : 0x%x\n", ptr[i].z_conds);
       printk(KERN_DEBUG "\n");
   }
}

int smrsim_query_zones(sector_t lba, 
                       int criteria, 
                       __u32 *num_zones, 
                       struct smrsim_zone_status *ptr)
{
   int   idx32;
   __u32 num32;
   __u32 zone_idx;

   if (!num_zones || !ptr) {
      printk(KERN_ERR "smrsim: null pointer passed through\n");
      return -EINVAL;
   }
   mutex_lock(&smrsim_zone_lock);
   zone_idx = lba >> SMR_BLOCK_SIZE_SHIFT 
                  >> SMR_ZONE_SIZE_SHIFT; 
   if (0 == *num_zones || SMR_NUMZONES < (*num_zones + zone_idx)) {
      mutex_unlock(&smrsim_zone_lock);
      printk(KERN_ERR "smrsim:: Number of zone out of range\n");
      return -EINVAL;
   }
   if (smrsim_dbg_log_enabled) {   
      smrsim_list_zone_status(zone_status, *num_zones, criteria);
   }
   if (criteria > 0) {
      idx32 = 0; 
      for (num32 = 0; num32 < *num_zones; num32++) {
         if ((num_sectors_zone() - zone_status[zone_idx + num32].z_write_ptr_offset)
              >= criteria) {
            memcpy((ptr + idx32), &zone_status[zone_idx + num32], 
                   sizeof(struct smrsim_zone_status));
            idx32++;
         }
      }
      *num_zones = idx32;
      mutex_unlock(&smrsim_zone_lock);
      return 0;      
   }
   switch (criteria) {
      case ZONE_MATCH_ALL:
         memcpy(ptr, &zone_status[zone_idx], *num_zones * 
            sizeof(struct smrsim_zone_status));
         break;
      case ZONE_MATCH_FULL:
         idx32 = 0; 
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if (Z_COND_FULL == zone_status[num32].z_conds) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      case ZONE_MATCH_NFULL:
         idx32 = 0;
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if ((Z_COND_CLOSED == zone_status[num32].z_conds) &&
                zone_status[num32].z_write_ptr_offset) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      case ZONE_MATCH_FREE:
         idx32 = 0;
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if ((Z_COND_EMPTY == zone_status[num32].z_conds)) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      case ZONE_MATCH_RNLY:
         idx32 = 0;
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if (Z_COND_RO == zone_status[num32].z_conds) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      case ZONE_MATCH_OFFL:
         idx32 = 0;
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if (Z_COND_OFFLINE == zone_status[num32].z_conds) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      #if 0  /* future support */
      case ZONE_MATCH_WNEC:
         idx32 = 0;
         for (num32 = zone_idx; num32 < SMR_NUMZONES; num32++) {
            if (zone_status[num32].z_write_ptr_offset !=
                zone_status[num32].z_checkpoint_offset ) {
               memcpy((ptr + idx32), &zone_status[num32], 
                       sizeof(struct smrsim_zone_status));
               idx32++;
               if (idx32 == *num_zones) {
                  break;
               }
            }
         }
         *num_zones = idx32;
         break;
      #endif 
      default:
         printk("smrsim: wrong query parameter\n");
   }
   mutex_unlock(&smrsim_zone_lock);
   return 0;
}
EXPORT_SYMBOL(smrsim_query_zones);

#ifdef SMRSIM_WP_RT
int smrsim_forward_wp_adjust(__u8 flag)
{
   if (flag > 1) {
      return -EINVAL;
   }
   smrsim_wp_adjust_flag = flag;
   return 0;
}
EXPORT_SYMBOL(smrsim_forward_wp_adjust);

int smrsim_backward_wp_reset(__u8 flag)
{
   if (flag > 1) {
      return -EINVAL;
   }
   smrsim_wp_reset_flag = flag;
   return 0;
}
EXPORT_SYMBOL(smrsim_backward_wp_reset);

int smrsim_border_across(__u64 param)
{
   __u32 loop;
   __u32 zone_idx = (__u32)(param >> 8);
   enum smrsim_rt_bodr code = (param & 0xff);
    
   switch (code) {
      case SMR_BODR_CROSS_CUR:
         zone_status[zone_idx].z_flag = code;
         break;
      case SMR_BODR_CROSS_OFF:
         for (loop = 0; loop < SMR_NUMZONES; loop++)
         {
            zone_status[loop].z_flag = code;
         }
         break;
      case SMR_BODR_CROSS_SEQ:
         for (loop = 0; loop < SMR_NUMZONES; loop++)
         {
            if (zone_status[loop].z_type == Z_TYPE_SEQUENTIAL) {
               zone_status[loop].z_flag = code;
            }
         }   
         break;
      default:
         printk("smrsim: wrong ioctl code for cross border evaluation\n");
   }
   return 0;
}
EXPORT_SYMBOL(smrsim_border_across);
#endif

int smrsim_ioctl(struct dm_target* ti,
                 unsigned int cmd,
                 unsigned long arg)
{
   smrsim_zbc_query          *zbc_query;
   struct smrsim_dev_config   pconf;
   struct smrsim_zone_status  pstatus;
   struct smrsim_stats       *pstats;
   int                        ret = 0;
   __u32                      size  = 0;
   __u64                      num64;
   __u32                      param = SMR_NUMZONES;
 
   smrsim_dev_idle_update();
   mutex_lock(&smrsim_ioct_lock);
   switch(cmd)
   {
       case IOCTL_SMRSIM_GET_LAST_RERROR:
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_GET_LAST_RERROR", 0);
          if (smrsim_get_last_rd_error(&param)) {
             printk(KERN_ERR "smrsim: get last rd error failed\n");
             goto ioerr;
          }
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_to_user((__u32 *)arg, &param, sizeof(__u32) )) {
             printk(KERN_ERR "smrsim: copy last rd error to user memory failed\n");
             goto ioerr;
	  }
          break;
       case IOCTL_SMRSIM_GET_LAST_WERROR:
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_GET_LAST_WERROR", 0);
          if (smrsim_get_last_wd_error(&param)) {
             printk(KERN_ERR "smrsim: Get last wd error failed\n");
             goto ioerr;
          }
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if(copy_to_user((__u32 *)arg, &param, sizeof(__u32) )) {
             printk(KERN_ERR "smrsim: copy last wd error to user memory failed\n");
             goto ioerr;
	  }
          break;
       /* 
        * zone ioctl
        */
       case IOCTL_SMRSIM_SET_LOGENABLE:
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_SET_LOGENABLE", 0);
          if (smrsim_set_log_enable(1)) {
             printk(KERN_ERR "smrsim: enable log failed\n");
             goto ioerr;
          }
          break;
       case IOCTL_SMRSIM_SET_LOGDISABLE:
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_SET_LOGDISABLE", 0);
          if (smrsim_set_log_enable(0)) {
             printk(KERN_ERR "smrsim: disable log failed\n");
             goto ioerr;
          }
          break;
       case IOCTL_SMRSIM_GET_NUMZONES:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (smrsim_get_num_zones(&param)) {
             printk(KERN_ERR "smrsim: get number of zones failed\n");
             goto ioerr;
          }
          if (copy_to_user((__u32 *)arg, &param, sizeof(__u32) )) {
             printk(KERN_ERR "smrsim: copy num of zones to user memory failed\n");
             goto ioerr;
	  }
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_GET_NUMZONES", param);
          break;
       case IOCTL_SMRSIM_GET_SIZZONEDEFAULT:
          if (smrsim_get_size_zone_default(&param)) {
             printk(KERN_ERR "smrsim: get zone size failed\n");
             goto ioerr;
          }
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_to_user((__u32 *)arg, &param, sizeof(__u32))) {
             printk(KERN_ERR "smrsim: Copy zone size to user memory failed\n");
             goto ioerr;
          }
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_GET_SIZZONEDEFAULT", param);
          break;
       case IOCTL_SMRSIM_SET_SIZZONEDEFAULT:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&param, (__u32 *)arg, sizeof(__u32))) {
             printk(KERN_ERR "smrsim: set zone size copy from user failed\n");
             goto ioerr;
          }
          if (smrsim_set_size_zone_default(param)) {
             printk(KERN_ERR "smrsim: set default zone size failed\n");
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_SET_SIZZONEDEFAULT", param);
          break;
       case IOCTL_SMRSIM_ZBC_RESET_ZONE:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&num64, (__u64 *)arg, sizeof(__u64))) {
             printk(KERN_ERR "smrsim: copy reset zone write pointer from user memory failed\n");
             goto ioerr;
          }
          if (smrsim_blkdev_reset_zone_ptr(num64)) {
             printk(KERN_ERR "smrsim: reset zone write pointer failed\n");
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          trace_smrsim_ioctl_evt("IOCTL_SMRSIM_ZBC_RESET_ZONE", num64);
          break;
       case IOCTL_SMRSIM_ZBC_QUERY:
           zbc_query = kzalloc(sizeof(smrsim_zbc_query), GFP_KERNEL);
           if (!zbc_query) {
              printk(KERN_ERR "smrsim: %s no enough emeory for zbc query\n", __FUNCTION__);
              goto ioerr;
           } 
           if ((__u64)arg == 0) {
              printk(KERN_ERR "smrsim: bad parameter\n");
              goto zfail; 
           }
           ret = copy_from_user(zbc_query, (smrsim_zbc_query*)arg,
                         sizeof(smrsim_zbc_query));
           if (ret) {
              printk(KERN_ERR "smrsim: %s copy from user for zbc query failed\n", __FUNCTION__);
              goto zfail;
           }
           if (zbc_query->num_zones == 0 || zbc_query->num_zones > SMR_NUMZONES) {
              printk(KERN_ERR "smrsim: Wrong parameter for the number of zones\n");
              goto zfail;
           }
           trace_smrsim_zbcquery_evt("IOCTL_SMRSIM_ZBC_QUERY", zbc_query->lba,
                                     zbc_query->criteria, zbc_query->num_zones);        
           size = sizeof(smrsim_zbc_query) + sizeof(struct smrsim_zone_status) *
                  (zbc_query->num_zones - 1);
           zbc_query = krealloc(zbc_query, size, GFP_KERNEL);
           if (!zbc_query) {
              printk(KERN_ERR "smrsim: %s no enough emeory for zbc query\n", __FUNCTION__);
              goto zfail;
           } 
           if (smrsim_query_zones(zbc_query->lba, zbc_query->criteria, 
              &zbc_query->num_zones, zbc_query->ptr)) {
              printk(KERN_ERR "smrsim: %s query zone status failed\n", __FUNCTION__);
              goto zfail;            
           }
           if (copy_to_user((__u32 *)arg, zbc_query, size)) {
              printk(KERN_ERR "smrsim: %s copy to user for zbc query failed\n", __FUNCTION__);
              goto zfail;
           }
           kfree(zbc_query);
           break;
        zfail:
           kfree(zbc_query);
           goto ioerr;
       /*
        * SMRSIM stats IOCTLs
        */
       case IOCTL_SMRSIM_GET_STATS:
          size = smrsim_stats_size();
          pstats = (struct smrsim_stats *)kzalloc(size, GFP_ATOMIC);
          if (!pstats) {
             printk(KERN_ERR "smrsim: no enough memory to hold stats\n");
             goto ioerr;
          }
          trace_smrsim_stats_evt("IOCTL_SMRSIM_GET_STATS", size);
          if (smrsim_get_stats(pstats)) {
             printk(KERN_ERR "smrsim: get stats failed\n");
             kfree(pstats);
             goto sfail;
          }
          if (smrsim_dbg_log_enabled) {
             smrsim_report_stats(pstats);
          }
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto sfail; 
          }
          if (copy_to_user((struct smrsim_stats*)arg, pstats, size)) {
             printk(KERN_ERR "smrsim: get stats failed as insufficient user memory\n");
             kfree(pstats);
             goto sfail;
          }
          kfree(pstats);
          break;
       sfail:
          kfree(pstats);
          goto ioerr;
       case IOCTL_SMRSIM_RESET_STATS:
          trace_smrsim_stats_evt("IOCTL_SMRSIM_RESET_STATS", 0);
          if (smrsim_reset_stats()) {
             printk(KERN_ERR "smrsim: reset stats failed\n"); 
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_RESET_ZONESTATS:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&num64, (__u64 *)arg, sizeof(__u64))) {
             printk(KERN_ERR "smrsim: copy reset zone lba from user memory failed\n");
             goto ioerr;
          }
          trace_smrsim_stats_evt("IOCTL_SMRSIM_RESET_ZONESTATS", num64);
          if (smrsim_reset_zone_stats(num64)) {
             printk(KERN_ERR "smrsim: reset zone stats on lba failed\n");
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       /*
        * SMRSIM config IOCTLs
        */
       case IOCTL_SMRSIM_RESET_DEFAULTCONFIG:
          trace_smrsim_conf_evt("IOCTL_SMRSIM_RESET_DEFAULTCONFIG", 0);
          if (smrsim_reset_default_config()) {
             goto ioerr;
          }
          #ifdef SMRSIM_WP_RT
          if (smrsim_backward_wp_reset(0)) {
             printk(KERN_ERR "smrsim: turn off reset flag failed\n");
             goto ioerr;
          }
          if (smrsim_forward_wp_adjust(0)) {
             printk(KERN_ERR "smrsim: turn off adjust flag failed\n");
             goto ioerr;
          }
          #endif
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_RESET_ZONECONFIG:
          trace_smrsim_zone_evt("IOCTL_SMRSIM_RESET_ZONECONFIG", 0);
          if (smrsim_reset_default_zone_config()) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_RESET_DEVCONFIG:
          trace_smrsim_dev_evt("IOCTL_SMRSIM_DEFAULTDEVCONFIG", 0);
          if (smrsim_reset_default_device_config()) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_GET_DEVCONFIG:
          if (smrsim_get_device_config(&pconf)) {
             goto ioerr;
          }
          trace_smrsim_dev_get_conf_evt("IOCTL_SMRSIM_GET_DEVCONFIG", &pconf);
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_to_user((struct smrsim_dev_config*)arg, &pconf,
		   sizeof(struct smrsim_dev_config) )) {
             goto ioerr;
          }
          break;
       case IOCTL_SMRSIM_SET_DEVRCONFIG:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pconf, (struct smrsim_dev_config*)arg,
	     sizeof(struct smrsim_dev_config))) {
             goto ioerr;
          }
          trace_smrsim_dev_set_conf_evt("IOCTL_SMRSIM_SET_DEV_RCONFIG", &pconf); 
          if (smrsim_set_device_rconfig(&pconf)) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_SET_DEVWCONFIG:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pconf, (struct smrsim_dev_config*)arg,
	     sizeof(struct smrsim_dev_config))) {
             goto ioerr;
          }
          trace_smrsim_dev_set_conf_evt("IOCTL_SMRSIM_SET_DEV_WCONFIG", &pconf); 
          if (smrsim_set_device_wconfig(&pconf)) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_SET_DEVRCONFIG_DELAY:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pconf, (struct smrsim_dev_config*)arg,
	     sizeof(struct smrsim_dev_config))) {
             goto ioerr;
          }
          trace_smrsim_dev_set_conf_evt("IOCTL_SMRSIM_SET_DEVRCONFIG_DELAY", &pconf); 
          if (smrsim_set_device_rconfig_delay(&pconf)) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_SET_DEVWCONFIG_DELAY:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pconf, (struct smrsim_dev_config*)arg,
	     sizeof(struct smrsim_dev_config))) {
             goto ioerr;
          }
          trace_smrsim_dev_set_conf_evt("IOCTL_SMRSIM_SET_DEVWCONFIG_DELAY", &pconf); 
          if (smrsim_set_device_wconfig_delay(&pconf)) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;

       case IOCTL_SMRSIM_CLEAR_ZONECONFIG:
          trace_smrsim_zone_evt("IOCTL_SMRSIM_CLEAR_ZONECONFIG", 0);
          if (smrsim_clear_zone_config()) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_ADD_ZONECONFIG:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: Bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pstatus,  (struct smrsim_zone_status*)arg,
                     sizeof(struct smrsim_zone_status))) {
             goto ioerr;
          }
          trace_smrsim_add_zone_conf_evt("IOCTL_SMRSIM_ADD_ZONECONFIG", &pstatus);
          if (smrsim_add_zone_config(&pstatus)) { 
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       case IOCTL_SMRSIM_MODIFY_ZONECONFIG:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim:error: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&pstatus, (struct smrsim_zone_status*)arg,
             sizeof(struct smrsim_zone_status))) {
             goto ioerr;
          }
          trace_smrsim_modify_zone_conf_evt("IOCTL_SMRSIM_MODIFY_ZONECONFIG", &pstatus);
          if (smrsim_modify_zone_config(&pstatus)) {
             goto ioerr;
          }
          smrsim_ptask.flag |= SMR_CONFIG_CHANGE;
          break;
       #ifdef SMRSIM_WP_RT
       /*
        * test and simulation only on write pointer violation handling
        */      
       case IOCTL_SMRSIM_BDWP_RESET:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&param, (__u32 *)arg, sizeof(__u32))) {
             printk(KERN_ERR "smrsim: wrong parameter.\n");
             goto ioerr;
          }
          if (smrsim_backward_wp_reset(param)) {
             printk(KERN_ERR "smrsim: turn reset flag failed\n");
             goto ioerr;
          }
          break;
       case IOCTL_SMRSIM_FDWP_ADJST:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&param, (__u32 *)arg, sizeof(__u32))) {
             printk(KERN_ERR "smrsim: wrong parameter.\n");
             goto ioerr;
          }
          if (smrsim_forward_wp_adjust(param)) {
             printk(KERN_ERR "smrsim: turn adjust flag failed\n");
             goto ioerr;
          }
          break;
       case IOCTL_SMRSIM_BORDER_CROSS:
          if ((__u64)arg == 0) {
             printk(KERN_ERR "smrsim: bad parameter\n");
             goto ioerr; 
          }
          if (copy_from_user(&num64, (__u64 *)arg, sizeof(__u64))) {
             printk(KERN_ERR "smrsim: wrong parameter.\n");
             goto ioerr;
          }
          if (smrsim_border_across(num64)) {
             printk(KERN_ERR "smrsim: turn border cross flag failed\n");
             goto ioerr;
          }
          break;      
       #endif

       default:
          break;
   }
   
   if (smrsim_ptask.flag & SMR_CONFIG_CHANGE) {
      wake_up_process(smrsim_ptask.pstore_thread);
   }
   mutex_unlock(&smrsim_ioct_lock);
   return 0;
   ioerr:
   mutex_unlock(&smrsim_ioct_lock);
   /* any other things */
   return -EFAULT;
}

static int smrsim_merge(struct dm_target* ti, 
                        struct bvec_merge_data* bvm,
                        struct bio_vec* biovec, 
                        int max_size)
{
   struct smrsim_c*      c = ti->private;
   struct request_queue* q = bdev_get_queue(c->dev->bdev);

   if (!q->merge_bvec_fn)
      return max_size;

   bvm->bi_bdev   = c->dev->bdev;
   bvm->bi_sector = smrsim_map_sector(ti, bvm->bi_sector);

   return min(max_size, q->merge_bvec_fn(q, bvm, biovec));
}

static int smrsim_iterate_devices(struct dm_target *ti,
                                  iterate_devices_callout_fn fn,
                                  void *data)
{
   struct smrsim_c* c = ti->private;

   return fn(ti, c->dev, c->start, ti->len, data);
}

static struct target_type smrsim_target = 
{
   .name            = "smrsim",
   .version         = {1, 0, 0},
   .module          = THIS_MODULE,
   .ctr             = smrsim_ctr,
   .dtr             = smrsim_dtr,
   .map             = smrsim_map,
   .status          = smrsim_status,
   .ioctl           = smrsim_ioctl,
   .merge           = smrsim_merge,
   .iterate_devices = smrsim_iterate_devices
};

static int __init dm_smrsim_init (void)
{
   int ret = 0;

   printk(KERN_INFO "smrsim: %s called\n", __FUNCTION__);

   ret = dm_register_target(&smrsim_target);
   if(0 > ret)
      printk(KERN_ERR "smrsim: register failed: %d", ret);

   return ret;
}

static void dm_smrsim_exit (void)
{
   dm_unregister_target(&smrsim_target);
}

module_init(dm_smrsim_init);
module_exit(dm_smrsim_exit);

MODULE_DESCRIPTION(DM_NAME "SMR Simulator");
MODULE_AUTHOR("Shanghua Wang <shanghua.wang@wdc.com>");
MODULE_LICENSE("GPL");
