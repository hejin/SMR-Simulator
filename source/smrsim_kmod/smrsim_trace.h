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
#undef TRACE_SYSTEM
#define TRACE_SYSTEM smrsim_trace

#if !defined(_SMRSIM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SMRSIM_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(smrsim_ctr_evt,

	TP_PROTO(char *dev_name, unsigned long long start_lba, char *evt),
	TP_ARGS(dev_name, start_lba, evt),
	TP_STRUCT__entry(
		__array(char,			dev_name,	128)
		__field(unsigned long long,	start_lba)
		__array(char,			evt,	128)
	),
	TP_fast_assign(
		strncpy(__entry->dev_name, dev_name, 128);
		__entry->start_lba = start_lba;
		strncpy(__entry->evt, evt, 128)
	),
	TP_printk("smrsim device name:%s start lba:%llu %s",
		__entry->dev_name, __entry->start_lba, __entry->evt)
);

TRACE_EVENT(smrsim_gen_evt,

	TP_PROTO(char *dev_name, char *evt),
	TP_ARGS(dev_name, evt),
	TP_STRUCT__entry(
		__array(char,	dev_name,	128)
		__array(char,	evt,		128)
	),
	TP_fast_assign(
		strncpy(__entry->dev_name, dev_name, 128);
		strncpy(__entry->evt, evt, 128)
	),
	TP_printk("smrsim device name:%s %s",
		__entry->dev_name, __entry->evt)
);

DECLARE_EVENT_CLASS(smrsim_ioctl_template,

	TP_PROTO(char *cmd, unsigned long long arg),
	TP_ARGS(cmd, arg),
	TP_STRUCT__entry(
		__array(char,			cmd,	128)
		__field(unsigned long long,	arg)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->arg = arg;
	),
	TP_printk("%s arg:%llu", __entry->cmd, __entry->arg)
);

DEFINE_EVENT(smrsim_ioctl_template, smrsim_ioctl_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

DEFINE_EVENT(smrsim_ioctl_template, smrsim_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

DEFINE_EVENT(smrsim_ioctl_template, smrsim_stats_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

DEFINE_EVENT(smrsim_ioctl_template, smrsim_conf_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

DEFINE_EVENT(smrsim_ioctl_template, smrsim_dev_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

DEFINE_EVENT(smrsim_ioctl_template, smrsim_zone_evt,
		TP_PROTO(char *cmd, unsigned long long arg), TP_ARGS(cmd, arg));

TRACE_EVENT(smrsim_zbcquery_evt,

	TP_PROTO(char *cmd, unsigned long long start_lba, 
		int criteria, unsigned int max_zones),
	TP_ARGS(cmd, start_lba, criteria, max_zones),
	TP_STRUCT__entry(
		__array(char,			cmd,	128)
		__field(unsigned long long,	start_lba)
		__field(int,			criteria)
		__field(unsigned int,		max_zones)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->start_lba = start_lba;
		__entry->criteria = criteria;
		__entry->max_zones = max_zones;
	),
	TP_printk("%s start_lba:%llu criteria:%d max_zones:%u",
		__entry->cmd, __entry->start_lba, __entry->criteria,
		__entry->max_zones)
);

TRACE_EVENT(smrsim_zone_stats_evt,

	TP_PROTO(char *cmd, struct smrsim_zone_stats *zone_stats, 
		unsigned long long lba),
	TP_ARGS(cmd, zone_stats, lba),
	TP_STRUCT__entry(
		__array(char,			cmd,	128)
		__field(unsigned int,		r_beyond_swp_count)
		__field(unsigned int,		r_span_zones_count)

		__field(unsigned int,		w_not_on_swp_count)
		__field(unsigned int,		w_span_zones_count)
		__field(unsigned int,		w_unaligned_count)
		__field(unsigned long long,	lba)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->r_beyond_swp_count = 
			zone_stats->out_of_policy_read_stats.beyond_swp_count;
		__entry->r_span_zones_count = 
			zone_stats->out_of_policy_read_stats.span_zones_count;
		__entry->w_not_on_swp_count = 
			zone_stats->out_of_policy_write_stats.not_on_swp_count;
		__entry->w_span_zones_count = 
			zone_stats->out_of_policy_write_stats.span_zones_count;
		__entry->w_unaligned_count = 
			zone_stats->out_of_policy_write_stats.unaligned_count;
		__entry->lba = lba;
        ),
	TP_printk("%s r_beyond_swp:%u r_span_zones:%u w_not_on_swp:%u w_span_zones:%u w_unaligned:%u lba:%llu",
		__entry->cmd, __entry->r_beyond_swp_count, __entry->r_span_zones_count, __entry->w_not_on_swp_count, 
		__entry->w_span_zones_count, __entry->w_unaligned_count, __entry->lba)
);
  
TRACE_EVENT(smrsim_device_stats_evt,

	TP_PROTO(char *cmd, struct smrsim_dev_stats *dev_stats),
	TP_ARGS(cmd, dev_stats),
	TP_STRUCT__entry(
		__array(char,		cmd,	128)
		__field(unsigned int,	dev_idle_time_max)
		__field(unsigned int,	dev_idle_time_min)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->dev_idle_time_max =
			dev_stats->idle_stats.dev_idle_time_max;
		__entry->dev_idle_time_min =
			dev_stats->idle_stats.dev_idle_time_min;
        ),
	TP_printk("%s dev_idle_time_max:%u dev_idle_time_min:%u",
		__entry->cmd, __entry->dev_idle_time_max, __entry->dev_idle_time_min 
	)
);

DECLARE_EVENT_CLASS(smrsim_dev_conf_template,

	TP_PROTO(char *cmd, struct smrsim_dev_config *dev_config),
	TP_ARGS(cmd, dev_config),
	TP_STRUCT__entry(
		__array(char,		cmd,	128)
		__field(unsigned int,	out_of_policy_read_flag)
		__field(unsigned int,	out_of_policy_write_flag)
		__field(unsigned int,	r_time_to_rmw_zone)
		__field(unsigned int,	w_time_to_rmw_zone)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->out_of_policy_read_flag =
			dev_config->out_of_policy_read_flag;
		__entry->out_of_policy_write_flag =
			dev_config->out_of_policy_write_flag;
		__entry->r_time_to_rmw_zone =
			dev_config->r_time_to_rmw_zone;
		__entry->w_time_to_rmw_zone =
			dev_config->w_time_to_rmw_zone;
        ),
	TP_printk("%s out_of_policy_read_flag:%u out_of_policy_write_flag:%u r_time_to_rmw_zone:%u w_time_to_rmw_zone:%u ",
		__entry->cmd, __entry->out_of_policy_read_flag, __entry->out_of_policy_write_flag,
		__entry->r_time_to_rmw_zone, __entry->w_time_to_rmw_zone
	)
);

DEFINE_EVENT(smrsim_dev_conf_template, smrsim_dev_get_conf_evt,
	TP_PROTO(char *cmd, struct smrsim_dev_config *dev_config), TP_ARGS(cmd, dev_config));

DEFINE_EVENT(smrsim_dev_conf_template, smrsim_dev_set_conf_evt,
	TP_PROTO(char *cmd, struct smrsim_dev_config *dev_config), TP_ARGS(cmd, dev_config));


DECLARE_EVENT_CLASS(smrsim_zone_conf_template,
	TP_PROTO(char *cmd, struct smrsim_zone_status *zone_status),
	TP_ARGS(cmd, zone_status),
	TP_STRUCT__entry(
		__array(char,		cmd,	128)
		__field(unsigned int,	z_start)
		__field(unsigned int,	z_length)
		__field(unsigned int,	z_write_ptr_offset)
		__field(unsigned int,	z_checkpoint_offset)
		__field(unsigned int,	z_conds)
                __field(unsigned int,   z_type)
	),
	TP_fast_assign(
		strncpy(__entry->cmd, cmd, 128);
		__entry->z_start = zone_status->z_start;
		__entry->z_length = zone_status->z_length;
		__entry->z_write_ptr_offset = zone_status->z_write_ptr_offset;
		__entry->z_checkpoint_offset = zone_status->z_checkpoint_offset;
		__entry->z_conds = zone_status->z_conds;
		__entry->z_type = zone_status->z_type;
	),
	TP_printk("%s z_start:%u z_length:%u z_write_ptr_offset:%u z_checkpoint_offset:%u z_conds:0x%x z_type:0x%x",
		__entry->cmd, __entry->z_start, __entry->z_length, __entry->z_write_ptr_offset,
		__entry->z_checkpoint_offset, __entry->z_conds, __entry->z_type
	) 
);

DEFINE_EVENT(smrsim_zone_conf_template, smrsim_add_zone_conf_evt,
	TP_PROTO(char *cmd, struct smrsim_zone_status *zone_status), TP_ARGS(cmd, zone_status));

DEFINE_EVENT(smrsim_zone_conf_template, smrsim_modify_zone_conf_evt,
	TP_PROTO(char *cmd, struct smrsim_zone_status *zone_status), TP_ARGS(cmd, zone_status));


TRACE_EVENT(smrsim_block_io_evt,

	TP_PROTO(char *dev_name, struct bio *bio),
	TP_ARGS(dev_name, bio),
	TP_STRUCT__entry(
		__array(char,		dev_name,	128)
		__field(unsigned long,	start_lba)
		__field(unsigned long,	length)
		__field(int,		io_dir)
	),
	TP_fast_assign(
		strncpy(__entry->dev_name, dev_name, 128);
                #if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
		__entry->start_lba = bio->bi_sector;
                #else
                __entry->start_lba = bio->bi_iter.bi_sector;
                #endif
		__entry->length = bio_sectors(bio);
		__entry->io_dir = bio_data_dir(bio);
	),
	TP_printk("%s start_lba:%lu length:%lu io direction:%s",
		__entry->dev_name, __entry->start_lba, __entry->length,
		__entry->io_dir? "write" : "read")
);

TRACE_EVENT(smrsim_zone_write_evt,

	TP_PROTO(unsigned long long zone_idx, unsigned int prev_swp, unsigned int curr_swp),
	TP_ARGS(zone_idx, prev_swp, curr_swp),
	TP_STRUCT__entry(
		__field(unsigned long long, 	zone_idx)
		__field(unsigned int,		prev_swp)
		__field(unsigned int,		curr_swp)
	),
	TP_fast_assign(
		__entry->zone_idx = zone_idx;
		__entry->prev_swp = prev_swp;
		__entry->curr_swp = curr_swp;
	),
	TP_printk("target zone index:%llu previous SWP:%u current SWP:%u",
		__entry->zone_idx, __entry->prev_swp, __entry->curr_swp)
);

TRACE_EVENT(smrsim_zone_read_evt,

	TP_PROTO(unsigned long long zone_idx, unsigned int swp),
	TP_ARGS(zone_idx, swp),
	TP_STRUCT__entry(
		__field(unsigned long long, 	zone_idx)
		__field(unsigned int,		swp)
	),
	TP_fast_assign(
		__entry->zone_idx = zone_idx;
		__entry->swp = swp;
	),
	TP_printk("target zone index:%llu swp:%u", __entry->zone_idx, __entry->swp)
);

DECLARE_EVENT_CLASS(smrsim_bio_check_template,

	TP_PROTO(char *op_err, unsigned int policy_flag, int err_code),
	TP_ARGS(op_err, policy_flag, err_code),
	TP_STRUCT__entry(
		__array(char,		op_err,	128)
		__field(unsigned int,	policy_flag)
		__field(int,		err_code)
	),
	TP_fast_assign(
		strncpy(__entry->op_err, op_err, 128);
		__entry->policy_flag = policy_flag;
		__entry->err_code = err_code;
	),
	TP_printk("%s policy_flag:%u err_code:%d", __entry->op_err,
		__entry->policy_flag, __entry->err_code)
);

DEFINE_EVENT(smrsim_bio_check_template, smrsim_bio_read_check_evt,
	TP_PROTO(char *op_err, unsigned int policy_flag, int err_code),
	TP_ARGS(op_err, policy_flag, err_code));

DEFINE_EVENT(smrsim_bio_check_template, smrsim_bio_write_check_evt,
	TP_PROTO(char *op_err, unsigned int policy_flag, int err_code),
	TP_ARGS(op_err, policy_flag, err_code));

DECLARE_EVENT_CLASS(smrsim_bio_oop_check_template,
	TP_PROTO(char *op_err, unsigned int policy_flag, unsigned int penalty, int err_code),
	TP_ARGS(op_err, policy_flag, penalty, err_code),
	TP_STRUCT__entry(
		__array(char,		op_err,	128)
		__field(unsigned int,	policy_flag)
		__field(unsigned int,	penalty)
		__field(int,		err_code)
	),
	TP_fast_assign(
		strncpy(__entry->op_err, op_err, 128);
		__entry->policy_flag = policy_flag;
		__entry->penalty = penalty;
		__entry->err_code = err_code;
	),
	TP_printk("%s policy_flag:%u penalty_time:%ums err_code:%d", __entry->op_err,
		__entry->policy_flag, __entry->penalty, __entry->err_code)
);

DEFINE_EVENT(smrsim_bio_oop_check_template, smrsim_bio_oop_read_check_evt,
	TP_PROTO(char *op_err, unsigned int policy_flag, unsigned int penalty, int err_code),
	TP_ARGS(op_err, policy_flag, penalty, err_code));

DEFINE_EVENT(smrsim_bio_oop_check_template, smrsim_bio_oop_write_check_evt,
	TP_PROTO(char *op_err, unsigned int policy_flag, unsigned int penalty, int err_code),
	TP_ARGS(op_err, policy_flag, penalty, err_code));

#endif /* _SMRSIM_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE smrsim_trace
#include <trace/define_trace.h>

