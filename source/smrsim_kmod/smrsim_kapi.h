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
#ifndef _SMRSIM_KAPI_H
#define _SMRSIM_KAPI_H

/*
 * SMRSIM_GET_LAST_WERROR
 *
 * Get the error result of the last write function call.
 *
 * Returns 0 if operation is successful, negative otherwise. 
 *
 */
int smrsim_get_last_wd_error(__u32 *last_error);

/*
 * SMRSIM_GET_LAST_RERROR
 *
 * Get the error result of the last read function call.
 *
 * Returns 0 if operation is successful, negative otherwise. 
 *
 */
int smrsim_get_last_rd_error(__u32 *last_error);

/*
 * SMRSIM_SET_LOGENABLE
 *
 * Turn off or on logging; 0 turns it off, non-zero enables
 *
 * Returns 0 if operation is successful, negative otherwise. 
 *
 */
int smrsim_set_log_enable(__u32 zero_is_disable);

/*
 * SMRSIM_GET_NUMZONES
 *
 * Get number of zones configured on device.
 *
 * Returns 0 if operation is successful, negative otherwise. 
 *
 */
int smrsim_get_num_zones(__u32 *num_zones);

/*
 * SMRSIM_GET_SIZZONEDEFAULT
 *
 * Get default size of zones configured.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_get_size_zone_default(__u32 *siz_zone);

/*
 * SMRSIM_SET_SIZZONEDEFAULT
 *
 * Set size of zones configured.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_set_size_zone_default(__u32 siz_zone);

/*
 * SMRSIM_ZBC_RESET_ZONE
 *
 * Reset the write pointer for a sequential write zone.
 *
 * Returns -EINVAL if the start_sector is not the beginning of a
 * sequential write zone.
 *
 */
int smrsim_blkdev_reset_zone_ptr(sector_t start_sector);

/*
 * SMRSIM_ZBC_QUERY
 *
 * Query smrsim zone list that matches the criteria specified by
 * free_sectors_criteria.  Matched zone data for at most max_zones will
 * be placed into the user supplied memory ret_zones in ascending LBA order.
 * The return value will be a kernel error code if negative, or the number 
 * of zones actually returned if there is no error.
 *
 * If free_sectors_criteria is positive, then return zones that have
 * at least that many sectors available to be written.  If it is zero,
 * then match all zones.  If free_sectors_criteria is negative, then
 * return the zones that match the following criteria:
 *
 * -1 ZONE_MATCH_FULL  Match all full zones
 * -2 ZONE_MATCH_NFULL  Match all zones where each zone isn't a full zone.
 *   (the zone has at least one written sector and is not full - zone
 *   condition internally is in CLOSED state according to ZACr08.)
 *
 * -3 ZONE_MATCH_FREE  Match all free zones
 *    (the zone has no written sectors)
 *
 * -4 ZONE_MATCH_RNLY  Match all read-only zones
 * -5 ZONE_MATCH_OFFL  Match all offline zones
 * -6 ZONE_MATCH_WNEC  Match all zones where the write ptr != the checkpoint ptr
 *
 * The negative values are taken from Table 4 of 14-010r1, with the
 * exception of -6, which is not in the draft spec --- but IMHO should
 * be :-) It is anticipated, though, that the kernel will keep this
 * info in in memory and so will handle matching zones which meet
 * these criteria itself, without needing to issue a ZBC command for
 * each call to blkdev_query_zones().
 */
int smrsim_query_zones(sector_t start_sector,
                       int free_sectors_criteria, 
                       __u32 *max_zones,
                       struct smrsim_zone_status *ret_zones);

/*
 * SMRSIM_FDWPADJST 
 * 
 * Turn on/off a flag: 0 turns off, 1 turns on
 *
 * The function is to trigger a built-in codes which adjust zone write pointer
 * when incoming lba is ahead with the actual write pointerposition. This is a simulation 
 * of a handling on unmodified or out of order but forward writing from file systems.
 * Turn on the flag for testing purpose only. It is turned off by default.
 *
 */ 
int smrsim_forward_wp_adjust(__u8 flag);

/*
 * SMRSIM_BDWPESET 
 * 
 * Turn on/off a flag: 0 turns off, 1 turns on
 *
 * The function is to trigger a built-in codes which reset zone write pointer
 * when incoming lba is at zone start point behind the actual write pointer position. 
 * This is a simulation of a handling on unmodified or out of order writing from file
 * systems. Or a research exercise codes. Turn on the flag for research purpose only. 
 * It is turned off by default.
 *
 */ 
int smrsim_backward_wp_reset(__u8 flag);


/*
 * SMRSIM_GET_STATS
 *
 * Get SMRSIM stats values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_get_stats(struct smrsim_stats *stats);

/*
 * SMRSIM_RESET_STATS
 *
 * Resets SMRSIM stats values to default (0).
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_reset_stats(void);

/*
 * SMRSIM_RESET_ZONESTATS
 *
 * Resets SMRSIM stats of a zone to default values (0).
 *
 * Returns 0 if operation is successful, -EINVAL if the start_sector is
 * not the beginning of a sequential write zone.
 *
 */
int smrsim_reset_zone_stats(sector_t start_sector);

/*
 * SMRSIM_GET_DEVSTATS
 *
 * Get SMRSIM device stats.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_get_device_stats(struct smrsim_dev_stats *device_stats);
/*
 * SMRSIM_GET_ZONESTATS
 *
 * Get SMRSIM stats of a zone.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 * Returns -EINVAL if the start_sector is not the beginning of a
 * sequential write zone.
 *
 */
int smrsim_get_zone_stats(sector_t start_sector,
                          struct smrsim_zone_stats *zone_stats);
/*
 * SMRSIM_RESET_DEFAULTCONFIG
 *
 * Reset SMRSIM config (zone and device) values to default.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_reset_default_config(void);

/*
 *
 * SMRSIM_RESET_ZONECONFIG
 *
 * Reset SMRSIM zone configuration to default (divide LBA space into zones
 * with the first and last zone CMR, the rest SMR)
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_reset_default_zone_config(void);

/*
 * SMRSIM_RESET_DEVCONFIG
 *
 * Reset SMRSIM device configuration to default.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_reset_default_device_config(void);

/*
 * SMRSIM_GET_DEVCONFIG
 *
 * Get SMRSIM device config values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_get_device_config(struct smrsim_dev_config *device_config);

/*
 * SMRSIM_SET_DEVRCONFIG
 *
 * Set read SMRSIM device config values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_set_device_rconfig(struct smrsim_dev_config *device_config);

/*
 * SMRSIM_SET_DEVWCONFIG
 *
 * Set write SMRSIM device config values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_set_device_wconfig(struct smrsim_dev_config *device_config);

/*
 * SMRSIM_SET_DEVRCONFIG_DELAY
 *
 * Set read SMRSIM device config values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_set_device_rconfig_delay(struct smrsim_dev_config *device_config);

/*
 * SMRSIM_SET_DEVWCONFIG_DELAY
 *
 * Set write SMRSIM device config values.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_set_device_wconfig_delay(struct smrsim_dev_config *device_config);

/*
 * SMRSIM_CLEAR_ZONECONFIG
 *
 * Clear all zones from SMRSIM (number of zones will be 0 on completion of this
 *  function).
 *
 * Returns 0 if operation is successful, negative otherwise.
 */
int smrsim_clear_zone_config(void);

/*
 * SMRSIM_ADD_ZONECONFIG
 *
 * Add one more zone starting at the end of last zone configuration in LBA space.
 *
 * Error if zone_config does not start at the end of current last zone config.
 * Error if zone_zonfig goes beyond device LBA space.
 * Error if zone type is not allowed by ZBC/ZAC spec.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_add_zone_config(struct smrsim_zone_status *zone_status);

/*
 * SMRSIM_MODIFY_ZONECONFIG
 *
 * Modify selected zone.
 *
 * Error if zone config does not exist or if zone type is not allowed by
 * ZBC/ZAC spec.
 *
 * Returns 0 if operation is successful, negative otherwise.
 *
 */
int smrsim_modify_zone_config(struct smrsim_zone_status *zone_status);

#endif
