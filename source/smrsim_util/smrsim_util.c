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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>

/* 
 * The following user app codes are utility tools and also show examples. 
 
 * User applications need to include appropriate header file to replace 
 * following redefinition. User/Kernel APIs will be separated with a library 
 * wrapup later.
 */
#define   u8       __u8
#define   u32      __u32
#define   u64      __u64
#define   sector_t __u64

#include "smrsim_types.h"
#include "smrsim_ioctl.h"

/*
 * Hold the retrieved zone status - user applications need to keep sync with
 * smrsim device with the most recent update for its accuracy. This is an 
 * example usage only.
 */
smrsim_zbc_query  *zbc_query_cache;


int smrsim_util_print_help()
{
    printf("\nsmrsim_util error: Invalid command line!\n\nHELP:\n\n");
    printf("smrsim_util /dev/mapper/<dm_device_name> <code> <seq> <arg>\n");
    printf("\nExample:\n\n");
    printf("Show last read error     : smrsim_util /dev/mapper/smrsim e 1\n");
    printf("Show last write error    : smrsim_util /dev/mapper/smrsim e 2\n");
    printf("Enable logging           : smrsim_util /dev/mapper/smrsim e 3\n");
    printf("Disable logging          : smrsim_util /dev/mapper/smrsim e 4\n");
    printf("\n");
    printf("Get number of zones      : smrsim_util /dev/mapper/smrsim z 1\n");
    printf("Get default zone size    : smrsim_util /dev/mapper/smrsim z 2\n");
    printf("Set default zone size    : smrsim_util /dev/mapper/smrsim z 3 <size_in_sectors>\n");
    printf("\n");
    printf("ZBC reset zone status    : smrsim_util /dev/mapper/smrsim z 4 <lba>\n");
    printf("ZBC reset zone status    : smrsim_util /dev/mapper/smrsim z 5 <zone_index>\n");
    printf("\n");
    printf("ZBC query zone status    : smrsim_util /dev/mapper/smrsim z 6 <number_of_zones>\n");
    printf("ZBC query zone status    : smrsim_util /dev/mapper/smrsim z 7 <lba>\n");
    printf("ZBC query zone status    : smrsim_util /dev/mapper/smrsim z 8 <zone_index>\n");
    printf("\n"); 
    printf("Get all zone stats       : smrsim_util /dev/mapper/smrsim s 1\n");
    printf("Get zone stats           : smrsim_util /dev/mapper/smrsim s 2 <number_of_zones>\n");
    printf("Get zone stats by idx    : smrsim_util /dev/mapper/smrsim s 3 <zone_index>\n");
    printf("Reset all zone stats     : smrsim_util /dev/mapper/smrsim s 4\n");
    printf("Reset zone stats by lba  : smrsim_util /dev/mapper/smrsim s 5 <lba>\n");
    printf("Reset zone stats by idx  : smrsim_util /dev/mapper/smrsim s 6 <zone_index>\n");
    printf("\n");
    printf("Set all default config   : smrsim_util /dev/mapper/smrsim l 1\n");
    printf("Set zone default config  : smrsim_util /dev/mapper/smrsim l 2\n");
    printf("Reset dev default config : smrsim_util /dev/mapper/smrsim l 3\n");
    printf("Get dev config           : smrsim_util /dev/mapper/smrsim l 4\n");
    printf("Set Out Of Policy Read   : smrsim_util /dev/mapper/smrsim l 5 <0|1> # 0:off 1:on\n");
    printf("Set Out Of Policy Write  : smrsim_util /dev/mapper/smrsim l 6 <0|1> # read:1 & write:1 means all passthrough\n");
    printf("Clear zone config        : smrsim_util /dev/mapper/smrsim l 7\n");
    printf("Add zone config          : smrsim_util /dev/mapper/smrsim l 8\n");
    printf("Modify zone config       : smrsim_util /dev/mapper/smrsim l 9 <zone_index>\n");
    printf("Set Read penalty delay   : smrsim_util /dev/mapper/smrsim l 10 <number_seconds>\n");
    printf("Set Write penalty delay  : smrsim_util /dev/mapper/smrsim l 11 <number_seconds>\n");
    printf("\n");
    printf("The followings are exercise commands for research purposes:\n");
    printf("\n");
    printf("Turn on/off WP reset     : smrsim_util /dev/mapper/smrsim h 1 <0|1> # 0:off 1:on\n");
    printf("Turn on/off WP adjust    : smrsim_util /dev/mapper/smrsim h 2 <0|1>\n");
    printf("\n");
    printf("Turn on SEQ border-cros  : smrsim_util /dev/mapper/smrsim h 3\n");
    printf("Turn on Zone border-cros : smrsim_util /dev/mapper/smrsim h 4 <zone_index>\n");
    printf("Turn off All border-cros : smrsim_util /dev/mapper/smrsim h 5\n");
    printf("\n\n");

    return 0;
}

void smrsim_err_iot(int fd, int seq)
{
    int num = 0;

    switch(seq)
    {
        case 1:
            if (!ioctl(fd, IOCTL_SMRSIM_GET_LAST_RERROR, &num)) {
                printf("Last read error: %d\n", num);
            } else {
                printf("Operation failed\n");
            }
            break;
        case 2:
            if (!ioctl(fd, IOCTL_SMRSIM_GET_LAST_WERROR, &num)) {
                printf("Last write error: %d\n", num);
            } else {
                printf("Operation failed\n");
            }
            break;
        case 3:
            if (!ioctl(fd, IOCTL_SMRSIM_SET_LOGENABLE)) {
                printf("Log enabled\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 4:
            if (!ioctl(fd, IOCTL_SMRSIM_SET_LOGDISABLE)) {
                printf("Log disabled\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        default:
            printf("ioctl error: Invalid command\n");
    }
}

void smrsim_report_zbc_query(smrsim_zbc_query *zbc_query)
{
   int i = 0;
   
   if (!zbc_query) {
      printf("zbc query null pointer\n");
      return;
   }
   if (zbc_query->num_zones == 0) {
       printf("Query result: no zone matched the query condition\n");
       return;   
   }
   printf("zbc query starting lba     : %llu\n", zbc_query->lba);
   printf("zbc query starting criteria: %d\n", zbc_query->criteria);
   printf("zbc query num of zones     : %u\n", zbc_query->num_zones);
   printf("\n");
   printf("zone status contents retrived as the following:\n");
   printf("\n");
   for (i = 0; i < zbc_query->num_zones; i++) {
      printf("zone index        : %llu\n", zbc_query->ptr[i].z_start);
      printf("zone length       : %u\n", zbc_query->ptr[i].z_length);
      printf("write offset      : %u\n", zbc_query->ptr[i].z_write_ptr_offset);
      /* printf("check point offset: %u\n", zbc_query->ptr[i].z_checkpoint_offset); */
      printf("zone type         : 0x%x\n", zbc_query->ptr[i].z_type);
      switch (zbc_query->ptr[i].z_conds) {
         case Z_COND_NO_WP:
            printf("zone condition    : ZONE NO WP\n");
            break;
         case Z_COND_IMP_OPEN:
            break;
         case Z_COND_EXP_OPEN:
            break;
         case Z_COND_EMPTY:
            printf("zone condition    : ZONE EMPTY\n");       
            break;
         case Z_COND_RO:
            printf("zone condition    : ZONE READ ONLY\n");
            break;
         case Z_COND_CLOSED:
            printf("zone condition    : ZONE CLOSED\n");
            break;
         case Z_COND_FULL:
            printf("zone condition    : ZONE FULL\n");
            break;
         case Z_COND_OFFLINE:
            printf("zone condition    : ZONE OFFLINE\n");
            break;
         default:
            break;
      }
      printf("zone control      : 0x%x\n", zbc_query->ptr[i].z_flag);
      printf("\n");
   }
}

void smrsim_zone_iot(int fd, int seq, char *argv[])
{
   u32 num32     = 0;
   u32 num_zones = 0;
   u64 num64     = 0;
   u64 lba       = 0;
   u8  num8      = 0;         
   smrsim_zbc_query  *zbc_query;

   switch(seq)
   {
      case 1:
         if (!ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num32)) {
            printf("Number of zones: %u\n", num32);
         } else {
            printf("operation failed\n");
         }
         break;
      case 2:

         if (!ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &num32)) {
            printf("Get zone default size: %u\n", num32);
         } else {
             printf("operation failed\n");
         }
         break;
      case 3:
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num32 = atoi(argv[4]);
         if (!ioctl(fd, IOCTL_SMRSIM_SET_SIZZONEDEFAULT, &num32)) {
            printf("Set zone default size: %u\n", num32);
         } else {
             printf("operation failed\n");
         }
         break;      
      case 4: 
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num64 = atol(argv[4]);
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_RESET_ZONE, &num64)) {
            printf("Reset zone: %llu\n", num64);
         } else {
             printf("operation failed\n");
         }
         break;
      case 5: 
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num_zones)) {
            printf("Unable to obtain zone number. Operation failed\n");
         }
         num32 = atoi(argv[4]);
         if (num32 >= num_zones) {
             printf("Zone index out of range\n");
             break;
         }         
         if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &num64)) {
             printf("Cannot get zone size. Operation failed.\n");
             break;
         }
         num64 = num32 * num64;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_RESET_ZONE, &num64)) {
            printf("Reset zone: %u\n", num32);
         } else {
             printf("Operation failed\n");
         }
         break;
      case 6: 
      case 7:
         if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num_zones)) {
             printf("Unable to get number of zones. operation failed\n");
             break;
         }
         if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &num64)) {
             printf("Cannot get zone size. Operation failed.\n");
             break;
         }
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         if (seq == 6) {  
            lba = 0;
            num32 = atoi(argv[4]);
            if (num32 == 0 || num32 > num_zones) {
               printf("The number of zones is out of boundary\n");
               smrsim_util_print_help();
               break;
            }
         } else {
            num32 = 1;
            lba = atol(argv[4]);
            if (lba >= num64 * num_zones) {
               printf("LBA is out of boundary.\n");
               smrsim_util_print_help();
               break;
            }
         }
         zbc_query = malloc(sizeof(smrsim_zbc_query) + (num32 - 1)
                     * sizeof(struct smrsim_zone_status));
         if (!zbc_query) {
             printf("No enough memory to continue\n");
             break;
         }
         /*
          * ZONE_MATCH_ALL
          */
         printf("\nMatch all:\n");
         zbc_query->criteria =  ZONE_MATCH_ALL; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         /*
          * ZONE_MATCH_FULL
          */
         printf("\nMatch full:\n");
         zbc_query->criteria =  ZONE_MATCH_FULL; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         /*
          * ZONE_MATCH_NFULL
          */
         printf("\nMatch to non-full zones:\n");
         zbc_query->criteria =  ZONE_MATCH_NFULL; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         /*
          * ZONE_MATCH_FREE
          */
         printf("\nMatch free Sequential Zones:\n");
         zbc_query->criteria =  ZONE_MATCH_FREE; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         /*
          * ZONE_MATCH_RNLY
          */
         printf("\nMatch read only:\n");
         zbc_query->criteria =  ZONE_MATCH_RNLY; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }

         /*
          * ZONE_MATCH_OFFL
          */
         printf("\nMatch offline:\n");
         zbc_query->criteria =  ZONE_MATCH_OFFL; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         #if 0  /* future support */
         /*
          * ZONE_MATCH_WNEC
          */
         printf("\nMatch zones WP isn't the same with check point:\n");
         zbc_query->criteria =  ZONE_MATCH_WNEC; 
         zbc_query->lba = lba;     
         zbc_query->num_zones = num32;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         printf("\n");
         free(zbc_query);
         break;
         #endif
      case 8:
         if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num_zones)) {
             printf("Unable to get number of zones. Operation failed.\n");
             break;
         }
         if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &num64)) {
             printf("Cannot get zone size. Operation failed.\n");
             break;
         }
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num32 = atoi(argv[4]);
         if (num32 >= num_zones) {
            printf("The zone index is out of boundary\n");
            smrsim_util_print_help();
            break;
         }
         zbc_query = malloc(sizeof(smrsim_zbc_query));
         if (!zbc_query) {
             printf("No enough memory to continue.\n");
             break;
         }
         /*
          * ZONE_MATCH_ALL
          */
         printf("\nMatch all:\n");
         zbc_query->criteria =  ZONE_MATCH_ALL; 
         zbc_query->lba = num32 * num64;     
         zbc_query->num_zones = 1;
         if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query)) {
            smrsim_report_zbc_query(zbc_query);
         } else {
             printf("Operation failed\n");
         }
         break; 
      default:
         printf("ioctl error: Invalid command.\n");
   }
}

void smrsim_report_zone_stats(struct smrsim_stats  *stats, u32 idx)
{
    if (!stats) {
        return;   
    }
    printf("Device idle time max: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_max);
    printf("Device idle time min: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_min);
    printf("zone[%u] smrsim out of policy read stats: beyond wp count: %u\n",
            idx, stats->zone_stats[idx].out_of_policy_read_stats.beyond_swp_count);
    printf("zone[%u] smrsim out of policy read stats: span zones count: %u\n",
            idx, stats->zone_stats[idx].out_of_policy_read_stats.span_zones_count);
    printf("zone[%u] smrsim out of policy write stats: not on wp count: %u\n",
            idx, stats->zone_stats[idx].out_of_policy_write_stats.not_on_swp_count);
    printf("zone[%u] smrsim out of policy write stats: span zones count: %u\n",
            idx, stats->zone_stats[idx].out_of_policy_write_stats.span_zones_count);
    printf("zone[%u] smrsim out of policy write stats: unaligned count: %u\n",
            idx, stats->zone_stats[idx].out_of_policy_write_stats.unaligned_count);
}

void smrsim_report_stats(struct smrsim_stats  *stats, u32 num32)
{
    u32 i = 0;
    printf("\nDevice idle time max: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_max);
    printf("Device idle time min: %u\n",
            stats->dev_stats.idle_stats.dev_idle_time_min);
   
    for (i = 0; i < num32; i++) {
       printf("zone[%u] smrsim out of policy read stats: beyond wp count: %u\n",
                i, stats->zone_stats[i].out_of_policy_read_stats.beyond_swp_count);
       printf("zone[%u] smrsim out of policy read stats: span zones count: %u\n",
                i, stats->zone_stats[i].out_of_policy_read_stats.span_zones_count);
       printf("zone[%u] smrsim out of policy write stats: not on wp count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.not_on_swp_count);
       printf("zone[%u] smrsim out of policy write stats: span zones count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.span_zones_count);
       printf("zone[%u] smrsim out of policy write stats: unaligned count: %u\n",
                i, stats->zone_stats[i].out_of_policy_write_stats.unaligned_count);
       printf("\n");
    }
}

void smrsim_stats_iot(int fd, int seq, char *argv[])
{
   struct smrsim_stats *stats;
   u32    num32     = 0;
   u32    num_zones = 0; 
   u64    num64     = 0;

   if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num_zones)) {
      printf("unable to get number of zones\n");
      return;
   }
   stats = (struct smrsim_stats *)malloc(sizeof(struct smrsim_dev_stats)
      + sizeof(u32) + sizeof(struct smrsim_zone_stats) * num_zones);
   if (!stats) {
      printf("No enough memory to continue.\n");
      return;
   }
   switch(seq)
   {
      case 1:
         if (!ioctl(fd, IOCTL_SMRSIM_GET_STATS, stats)) {
            printf("Get Stats:\n");
            smrsim_report_stats(stats, num_zones);
         } else {
            printf("Operation failed\n");
         }
         break;
      case 2:
         if (argv[4] == NULL) {
            smrsim_util_print_help();
            break;
         }
         num32 = atoi(argv[4]);
         if (num32 > num_zones) {
            printf("Too much zones specified\n");
            break;
         }
         if (!ioctl(fd, IOCTL_SMRSIM_GET_STATS, stats)) {
            printf("Get Stats:\n");
            smrsim_report_stats(stats, num32);
         } else {
            printf("Operation failed\n");
         }
         break;
      case 3:
         if (argv[4] == NULL) {
            smrsim_util_print_help();
            break;
         }
         num32 = atoi(argv[4]);
         if (num32 >= num_zones) {
            printf("Zone index out of range\n");
            break;
         }
         if (!ioctl(fd, IOCTL_SMRSIM_GET_STATS, stats)) {
            printf("Get Stats:\n");
            smrsim_report_zone_stats(stats, num32);
         } else {
            printf("Operation failed.\n");
         }
         break;  
      case 4:
         if (!ioctl(fd, IOCTL_SMRSIM_RESET_STATS)) {
            printf("Stats reset success\n");
         } else {
            printf("Operation failed\n");
         }
         break;
      case 5:
         if (argv[4] == NULL) {
            smrsim_util_print_help();
            break;
         }
         num64 = atol(argv[4]);
         if (!ioctl(fd, IOCTL_SMRSIM_RESET_ZONESTATS, &num64)) {
            printf("Zone stats reset Success\n");
         } else {
            printf("operation failed\n");
         }
         break;
      case 6:
         if (argv[4] == NULL) {
            smrsim_util_print_help();
            break;
         }
         num32 = atoi(argv[4]);
         if (num32 >= num_zones) {
            printf("zone index out of range\n");
            break;
         }
         if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &num64)) {
            printf("Cannot get zone size. Operation failed.\n");
            break;
         }
         num64 = num32 * num64;
         if (!ioctl(fd, IOCTL_SMRSIM_RESET_ZONESTATS, &num64)) {
            printf("Zone stats reset Success\n");
         } else {
            printf("Operation failed\n");
         }
         break;
      default:
         printf("ioctl error: Invalid command.\n");
   }
   free(stats);
}

static void smrsim_report_devconf(struct smrsim_dev_config *dev_conf)
{
   printf("smrsim dev out of policy read flag    : %u\n", 
          dev_conf->out_of_policy_read_flag);
   printf("smrsim dev out of policy write flag   : %u\n", 
          dev_conf->out_of_policy_write_flag);
   printf("smrsim dev out of policy read penalty : %u miliseconds\n",
          dev_conf->r_time_to_rmw_zone);
   printf("smrsim dev out of policy write penalty: %u miliseconds\n",
          dev_conf->w_time_to_rmw_zone);
}

u32 smrsim_num_seq_zones(smrsim_zbc_query *zbc_query_cache)
{
   u32 index;
   u32 cnt = 0;

   if (!zbc_query_cache && !zbc_query_cache->num_zones) {
      printf("No zone exist yet. Start to create it from the begining.\n");
      return cnt;   
   }
   for (index = 0; index < zbc_query_cache->num_zones; index++) {
      if (zbc_query_cache->ptr[index].z_type == Z_TYPE_SEQUENTIAL) {
         cnt++;
      }
   }
   return cnt;
}

void smrsim_config_iot(int fd, int seq, char *argv[])
{
    u32    num32  = 0; 
    u64    num64  = 0;
    u32    size32 = 0;
    u32    n_zone = 0;
    int    option = 0;
    u32    s_cnt  = 0;
    u8     flag   = 0; 
    enum smrsim_zone_conditions cond;

    struct smrsim_dev_config  dev_conf;
    struct smrsim_zone_status zone_status;

    if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num32)) {
        printf("Unable to get number of zones.\n");
        return;
    }   
    switch(seq)
    {
        case 1:    
            if (!ioctl(fd, IOCTL_SMRSIM_RESET_DEFAULTCONFIG)) {
                printf("Set smrsim default config success\n");
            } else {
                printf("Operation failed.\n");
            }
            break;
        case 2:    
            if (!ioctl(fd, IOCTL_SMRSIM_RESET_ZONECONFIG)) {
                printf("Set smrsim default zone config success\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 3:    
            if (!ioctl(fd, IOCTL_SMRSIM_RESET_DEVCONFIG)) {
                printf("Reset smrsim default dev config success\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 4:
            memset(&dev_conf, 0, sizeof(struct smrsim_dev_config));
            if (!ioctl(fd, IOCTL_SMRSIM_GET_DEVCONFIG, &dev_conf)) {
                printf("Get dev config Success\n");
                smrsim_report_devconf(&dev_conf);
            } else {
                printf("Operation failed\n");
            }
            break;
        case 5:
            memset(&dev_conf, 0, sizeof(struct smrsim_dev_config));
            if (argv[4] == NULL) {
                smrsim_util_print_help();
                break;
            }
            num32 = atoi(argv[4]);
            if (num32 != 0 && num32 != 1) {
                printf("Parameter position 4 should be 0 or 1\n");
                break;
            }                
            dev_conf.out_of_policy_read_flag = num32;
            if (!ioctl(fd, IOCTL_SMRSIM_SET_DEVRCONFIG, &dev_conf)) {
                printf("Set dev config Success\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 6:
            memset(&dev_conf, 0, sizeof(struct smrsim_dev_config));
            if (argv[4] == NULL) {
                smrsim_util_print_help();
                break;
            }
            num32 = atoi(argv[4]);
            if (num32 != 0 && num32 != 1) {
                printf("Parameter position 4 should be 0 or 1\n");
                break;
            }                
            dev_conf.out_of_policy_write_flag = num32;
            if (!ioctl(fd, IOCTL_SMRSIM_SET_DEVWCONFIG, &dev_conf)) {
                printf("Set dev config Success\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 7:
            printf("\n\nWarning: This is a destructive operation. All zones will be cleaned up.\n");
            printf("There will be block io error showing up that other layers try to access the zone\n");
            printf("volume already cleaned. Exercise the operation with purpose and ignore the errors.\n");
            printf(" Reset the config to the default will prevent the errors happens again.\n\n");
            if (!ioctl(fd, IOCTL_SMRSIM_CLEAR_ZONECONFIG)) {
                printf("Clear zone config config success\n\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 8:
            s_cnt = 0;
            flag = 0;
            do {
                if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &num32)) {
                    printf("Get number of zones failed\n");
                    break;
                }
                printf("Current number of zones: %u\n", num32);
                if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &size32)) {
                    printf("Unable to get zone size\n");
                }
                if (!flag && num32) {
                    flag = 1;
                    zbc_query_cache = malloc(sizeof(smrsim_zbc_query) + (num32 - 1)
                                 * sizeof(struct smrsim_zone_status));
                    if (!zbc_query_cache) {
                        printf("No enough memory to continue.\n");
                        break;
                    }
                    zbc_query_cache->criteria =  ZONE_MATCH_ALL; 
                    zbc_query_cache->lba = 0;
                    zbc_query_cache->num_zones = num32;
                    if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query_cache)) {
                        s_cnt = smrsim_num_seq_zones(zbc_query_cache);
                    } else {
                        printf("Query zone operation failed\n");
                        free(zbc_query_cache);
                        break;
                    }
                    free(zbc_query_cache);
                }       
                memset(&zone_status, 0, sizeof(zone_status));
                zone_status.z_start = num32 * size32;
                zone_status.z_length = size32;
                printf("Input Zone Type - 1 for Conventional 2 for Sequential:\n");
                scanf("%d", &option);
                if (option != 1 && option != 2) {
                    printf("Wrong parameter: %d - try again\n", option);
                    break;
                } 
                if (option == 1) {
                    zone_status.z_type = Z_TYPE_CONVENTIONAL;
                    zone_status.z_conds = Z_COND_NO_WP; 
                } else if (option == 2) {
                    s_cnt++;
                    zone_status.z_type = Z_TYPE_SEQUENTIAL;
                    zone_status.z_conds = Z_COND_EMPTY;
                }
                if (!ioctl(fd, IOCTL_SMRSIM_ADD_ZONECONFIG, &zone_status)) {
                    printf("Add zone config success\n");
                } else {
                    printf("No more capacity to add more zones.\n");
                    break;
                }
                printf("Add Zone: [continue: 1  discontinue: 0]\n");
                scanf("%d", &option);
                if (option == 0 && s_cnt == 0) {
                    printf("Warning: Detected all zones are CMR zones. Make sure there is at least 1 sequential zone.\n");
                    printf("Add a sequential zone or modify at least one zone to be sequential to match to SMR requirement.\n\n");
                }
            } while (option == 1);
            break;
        case 9:
            if (argv[4] == NULL) {
                smrsim_util_print_help();
                break;
            }
            num32 = atoi(argv[4]);
            s_cnt = 0;
            if (ioctl(fd, IOCTL_SMRSIM_GET_NUMZONES, &n_zone)) {
                printf("Unable to get number of zones\n");
                break;
            }
            if (num32 >= n_zone) {
               printf("Zone index is out of range\n");
               break;
            }
            if (n_zone) {
                zbc_query_cache = malloc(sizeof(smrsim_zbc_query) + (n_zone - 1)
                                 * sizeof(struct smrsim_zone_status));
                if (!zbc_query_cache) {
                    printf("No enough memory\n");
                    break;
                }
                zbc_query_cache->criteria =  ZONE_MATCH_ALL; 
                zbc_query_cache->lba = 0;
                zbc_query_cache->num_zones = n_zone;
                if (!ioctl(fd, IOCTL_SMRSIM_ZBC_QUERY, zbc_query_cache)) {
                    s_cnt = smrsim_num_seq_zones(zbc_query_cache);
                } else {
                    printf("No zone found. Operation failed.\n");
                    free(zbc_query_cache);
                    break;
                }
                free(zbc_query_cache);
            }
            if (ioctl(fd, IOCTL_SMRSIM_GET_SIZZONEDEFAULT, &size32)) {
                printf("Unable to get zone default size\n");
            } 
            memset(&zone_status, 0, sizeof(zone_status));
            zone_status.z_start = num32;
            zone_status.z_length = size32;
            printf("Input zone type [1: conventional 2: sequential]: ");
            scanf("%d", &option);
            if (option != 1 && option != 2) {
                printf("wrong parameter - try again\n");
                break;
            } 
            if (option == 1) {
                zone_status.z_type = Z_TYPE_CONVENTIONAL;
                zone_status.z_conds = Z_COND_NO_WP; 
            } else if (option == 2) {
                s_cnt++;
                zone_status.z_type = Z_TYPE_SEQUENTIAL;
                printf("Input zone condition [EMPTY:0x01 RO:0x0D OFFLINE:0x0F FULL:0x0E CLOSED:0x04]: ");
                scanf("%x", &cond);
                if (cond == Z_COND_IMP_OPEN || cond == Z_COND_EXP_OPEN) {
                    printf("Wrong parameter - try again\n");
                    break;
                } 
                zone_status.z_conds = cond;
                if (cond == Z_COND_EMPTY) {
                    zone_status.z_write_ptr_offset = 0;
                    zone_status.z_checkpoint_offset = 0;
                }
            }
            if (!s_cnt) {
                printf("Detected all zones are CMR zones. Need at least one Sequential zone. Abort.\n\n");
                break;
            }
            if (!ioctl(fd, IOCTL_SMRSIM_MODIFY_ZONECONFIG, &zone_status)) {
                printf("Modify zone config success\n");
            } else {
                printf("Modify zone operation failed\n");
            }
            break;
        case 10:
            memset(&dev_conf, 0, sizeof(struct smrsim_dev_config));
            if (argv[4] == NULL) {
                smrsim_util_print_help();
                break;
            }
            num32 = atoi(argv[4]);           
            dev_conf.r_time_to_rmw_zone = num32 * 1000;
            if (!ioctl(fd, IOCTL_SMRSIM_SET_DEVRCONFIG_DELAY, &dev_conf)) {
                printf("Set dev config read penalty Success\n");
            } else {
                printf("Operation failed\n");
            }
            break;
        case 11:
            memset(&dev_conf, 0, sizeof(struct smrsim_dev_config));
            if (argv[4] == NULL) {
                smrsim_util_print_help();
                break;
            }
            num32 = atoi(argv[4]);       
            dev_conf.w_time_to_rmw_zone = num32 * 1000;
            if (!ioctl(fd, IOCTL_SMRSIM_SET_DEVWCONFIG_DELAY, &dev_conf)) {
                printf("Set dev config write penalty Success\n");
            } else {
                printf("Operation failed\n");
            }
            break;

        default:
            printf("ioctl error: Invalid command\n");
    }
}

/* 
 * example to config zone cross border control - special exercises  
 */
void smrsim_zone_rt(int fd, int seq, char *argv[])
{
   u64 num64   = 0;
   u8  num8    = 0;

   switch(seq)
   {
      case 1:
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num8 = atoi(argv[4]);
         if (!ioctl(fd, IOCTL_SMRSIM_BDWP_RESET, &num8)) {
            if (num8) {
                printf("Reset zone pointer flag on\n");
            } else {
                printf("Reset zone pointer flag off\n");
            }
         } else {
             printf("Reset WP operation failed\n");
         }
         break;
      case 2: 
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num8 = atoi(argv[4]);
         if (!ioctl(fd, IOCTL_SMRSIM_FDWP_ADJST, &num8)) {
            if (num8) {
                printf("Adjust zone pointer flag on\n");
            } else {
                printf("Adjust zone pointer flag off\n");
            }
         } else {
             printf("Adjust zone pointer operation failed\n");
         }
         break;
      case 3: 
         num64 = 0x02;
         if (!ioctl(fd, IOCTL_SMRSIM_BORDER_CROSS, &num64)) {
            printf("Allow all SEQ zone border across\n");
         } else {
             printf("Operation failed\n");
         }
         break;
      case 4: 
         if (argv[4] == NULL) {
             smrsim_util_print_help();
             break;
         }
         num64 = ((atol(argv[4]) << 8) | 0x04);
         if (!ioctl(fd, IOCTL_SMRSIM_BORDER_CROSS, &num64)) {
            printf("Allow the zone border across\n");
         } else {
             printf("Operation failed\n");
         }
         break;
      case 5: 
         num64 = 0;
         if (!ioctl(fd, IOCTL_SMRSIM_BORDER_CROSS, &num64)) {
            printf("Clear all zone border across\n");
         } else {
             printf("Operation failed\n");
         }
         break;
      default:
         printf("ioctl error: Invalid command\n");
         break;
   }
} 



int main(int argc, char* argv[])
{
   int   fd;
   int   seq;
   char  code;

   if (4 != argc && 5 != argc) {
      return smrsim_util_print_help();
   }
   code = argv[2][0];
   seq = atoi(argv[3]);
   fd = open(argv[1], O_RDWR);

   if (-1 == fd)
   {
      printf("Error: %s open failed\n", argv[1]);
      return -1;
   }
   switch (code) {
      case 'e':
         smrsim_err_iot(fd, seq);
         break;
      case 'z':
         smrsim_zone_iot(fd, seq, argv);
         break;
      case 's':
         smrsim_stats_iot(fd, seq, argv);
         break;
      case 'l':
         smrsim_config_iot(fd, seq, argv);
         break;
      case 'h':
         smrsim_zone_rt(fd, seq, argv);
         break;
      default:
         return smrsim_util_print_help(); 
   } 
   close(fd);
   return 0;
}
