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
#ifndef _SMRSIM_IOCTL_H
#define _SMRSIM_IOCTL_H

#include "smrsim_types.h"

/*
 *
 * SMRSIM zone status IOCTLs
 *
 */
#define IOCTL_SMRSIM_GET_NUMZONES         _IOR('z',  1, __u32 *)
#define IOCTL_SMRSIM_GET_SIZZONEDEFAULT   _IOR('z',  2, __u32 *)
#define IOCTL_SMRSIM_SET_SIZZONEDEFAULT   _IOW('z',  3, __u32 *)
#define IOCTL_SMRSIM_ZBC_RESET_ZONE       _IOW('z',  4, __u64 *)
#define IOCTL_SMRSIM_ZBC_QUERY            _IOWR('z', 5, smrsim_zbc_query *)

/*
 *
 * SMRSIM zone rule control IOCTLs - rule changes for research exercises
 *
 */
#define IOCTL_SMRSIM_BDWP_RESET          _IOW('h',  1, __u8 *)
#define IOCTL_SMRSIM_FDWP_ADJST          _IOW('h',  2, __u8 *)
#define IOCTL_SMRSIM_BORDER_CROSS        _IOW('h',  3, __u64 *)

/*
 *
 * SMRSIM zone statistics IOCTLs
 *
 */
#define IOCTL_SMRSIM_GET_STATS            _IOR('s',  1, struct smrsim_stats *)
#define IOCTL_SMRSIM_RESET_STATS          _IO('s',   2)
#define IOCTL_SMRSIM_RESET_ZONESTATS      _IOW('s',  3, __u64 *)

/*
 *
 * SMRSIM zone config IOCTLs
 *
 */
#define IOCTL_SMRSIM_RESET_DEFAULTCONFIG  _IO('l',   1)
#define IOCTL_SMRSIM_RESET_ZONECONFIG     _IO('l',   2)
#define IOCTL_SMRSIM_RESET_DEVCONFIG      _IO('l',   3)
#define IOCTL_SMRSIM_GET_DEVCONFIG        _IOR('l',  4, struct smrsim_dev_config*)
#define IOCTL_SMRSIM_SET_DEVRCONFIG       _IOW('l',  5, struct smrsim_dev_config*)
#define IOCTL_SMRSIM_SET_DEVWCONFIG       _IOW('l',  6, struct smrsim_dev_config*)
#define IOCTL_SMRSIM_CLEAR_ZONECONFIG     _IO('l',   7)
#define IOCTL_SMRSIM_ADD_ZONECONFIG       _IOW('l',  8, struct smrsim_zone_status*)
#define IOCTL_SMRSIM_MODIFY_ZONECONFIG    _IOW('l',  9, struct smrsim_zone_status*)
#define IOCTL_SMRSIM_SET_DEVRCONFIG_DELAY _IOW('l',  10, struct smrsim_dev_config*)
#define IOCTL_SMRSIM_SET_DEVWCONFIG_DELAY _IOW('l',  11, struct smrsim_dev_config*)

/*
 *
 * SMRSIM debug error IOCTLs
 *
 */
#define IOCTL_SMRSIM_GET_LAST_RERROR      _IOR('e',  1, __u32 *)
#define IOCTL_SMRSIM_GET_LAST_WERROR      _IOR('e',  2, __u32 *)
#define IOCTL_SMRSIM_SET_LOGENABLE        _IO('e',   3)
#define IOCTL_SMRSIM_SET_LOGDISABLE       _IO('e',   4)

#endif
