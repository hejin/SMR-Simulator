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
#ifndef _SMRSIM_TYPE_H
#define _SMRSIM_TYPE_H

#define SMRSIM_VERSION(a, b, c)  ((a << 16) | (b << 8) | c)

enum smrsim_zone_conditions {
   Z_COND_NO_WP           = 0x00,
   Z_COND_EMPTY           = 0x01,
   Z_COND_IMP_OPEN        = 0x02,
   Z_COND_EXP_OPEN        = 0x03,
   Z_COND_CLOSED          = 0x04,
   Z_COND_RO              = 0x0D,
   Z_COND_FULL            = 0x0E, 
   Z_COND_OFFLINE         = 0x0F
};

enum smrsim_zone_type {
   Z_TYPE_RESERVED     = 0x00,
   Z_TYPE_CONVENTIONAL = 0x01,
   Z_TYPE_SEQUENTIAL   = 0x02,
   Z_TYPE_PREFERRED    = 0x03
};

struct smrsim_zone_status 
{
  sector_t  z_start;             /* blocks                  */
  __u32     z_length;            /* sectors                 */
  __u32     z_write_ptr_offset;  /* sectors                 */
  __u32     z_checkpoint_offset; /* TBD - future support    */
  __u16     z_conds;             /* conditions              */
  __u8      z_type;              /* type                    */
  __u8      z_flag;              /* control                 */
};

typedef struct
{
  __u64                      lba;          /* IN            */
  __u32                      num_zones;    /* IN/OUT        */
  int                        criteria;     /* IN            */
  struct smrsim_zone_status  ptr[1];       /* OUT           */

} smrsim_zbc_query;

struct smrsim_state_header
{
  __u32  magic;
  __u32  length;
  __u32  version;
  __u32  crc32;   
};

struct smrsim_idle_stats
{
    __u32  dev_idle_time_max;
    __u32  dev_idle_time_min;
};

struct smrsim_dev_stats 
{
    struct smrsim_idle_stats  idle_stats;
};

struct smrsim_out_of_policy_read_stats 
{
    __u32  beyond_swp_count;
    __u32  span_zones_count;
};

struct smrsim_out_of_policy_write_stats 
{
    __u32  not_on_swp_count;
    __u32  span_zones_count;
    __u32  unaligned_count;
};

struct smrsim_zone_stats
{
    struct smrsim_out_of_policy_read_stats   out_of_policy_read_stats;
    struct smrsim_out_of_policy_write_stats  out_of_policy_write_stats;
};

struct smrsim_stats 
{
   struct smrsim_dev_stats  dev_stats;
   __u32                    num_zones;
   struct smrsim_zone_stats zone_stats[1];
};

struct smrsim_dev_config
{
  /*
   * Default 0 to reject with error, 1 to add latency and satisfy request.
   */
  __u32 out_of_policy_read_flag;
  __u32 out_of_policy_write_flag;

  __u16 r_time_to_rmw_zone;   /* Default value is 4000ms */
  __u16 w_time_to_rmw_zone;   /* Default value is 4000ms */
};

struct smrsim_config
{
   struct smrsim_dev_config dev_config;
};


struct smrsim_state
{
   struct smrsim_state_header header;
   struct smrsim_config       config;
   struct smrsim_stats        stats;
};

/*
 * see ZBCQUERY comments below for define details
 */
enum smrsim_zbcquery_criteria {
   ZONE_MATCH_ALL  =           0, /* Match all zones                       */
   ZONE_MATCH_FULL =          -1, /* Match all zones full                  */
   ZONE_MATCH_NFULL =         -2, /* Match all zones not full              */
   ZONE_MATCH_FREE =          -3, /* Match all zones free                  */
   ZONE_MATCH_RNLY =          -4, /* Match all zones read-only             */
   ZONE_MATCH_OFFL =          -5, /* Match all zones offline               */
   ZONE_MATCH_WNEC =          -6  /* Match all zones wp != checkpoint ptr  */
};

#endif
