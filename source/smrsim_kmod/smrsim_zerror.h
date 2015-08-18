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
#ifndef _SMRSIM_DEBUG_H
#define _SMRSIM_DEBUG_H

#define SMR_ERR_READ_BORDER       -200
#define SMR_ERR_READ_POINTER      -202

#define SMR_ERR_WRITE_RO          -220
#define SMR_ERR_WRITE_FULL        -224
#define SMR_ERR_WRITE_BORDER      -226
#define SMR_ERR_WRITE_POINTER     -228
#define SMR_ERR_WRITE_ALIGN       -230

#define SMR_ERR_OUT_RANGE         -240
#define SMR_ERR_OUT_OF_POLICY     -242
#define SMR_ERR_ZONE_OFFLINE      -244
#define SMR_DM_IO_ERR             -246

#endif
