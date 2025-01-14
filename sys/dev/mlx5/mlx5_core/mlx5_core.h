/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/dev/mlx5/mlx5_core/mlx5_core.h 290650 2015-11-10 12:20:22Z hselasky $
 */

#ifndef __MLX5_CORE_H__
#define __MLX5_CORE_H__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#define DRIVER_NAME "mlx5_core"
#define DRIVER_VERSION "1.23.0 (03 Mar 2015)"
#define DRIVER_RELDATE "03 Mar 2015"

extern int mlx5_core_debug_mask;

#define mlx5_core_dbg(dev, format, ...)					\
	pr_debug("%s:%s:%d:(pid %d): " format,				\
		 (dev)->priv.name, __func__, __LINE__, curthread->td_proc->p_pid,	\
		 ##__VA_ARGS__)

#define mlx5_core_dbg_mask(dev, mask, format, ...)			\
do {									\
	if ((mask) & mlx5_core_debug_mask)				\
		mlx5_core_dbg(dev, format, ##__VA_ARGS__);		\
} while (0)

#define mlx5_core_err(dev, format, ...)					\
	printf("mlx5_core: ERR: ""%s:%s:%d:(pid %d): " format, \
	       (dev)->priv.name, __func__, __LINE__, curthread->td_proc->p_pid, \
	       ##__VA_ARGS__)

#define mlx5_core_warn(dev, format, ...)				\
	printf("mlx5_core: WARN: ""%s:%s:%d:(pid %d): " format, \
		(dev)->priv.name, __func__, __LINE__, curthread->td_proc->p_pid, \
		##__VA_ARGS__)

enum {
	MLX5_CMD_DATA, /* print command payload only */
	MLX5_CMD_TIME, /* print command execution time */
};

int mlx5_query_hca_caps(struct mlx5_core_dev *dev);
int mlx5_query_board_id(struct mlx5_core_dev *dev);
int mlx5_cmd_init_hca(struct mlx5_core_dev *dev);
int mlx5_cmd_teardown_hca(struct mlx5_core_dev *dev);

void mlx5e_init(void);
void mlx5e_cleanup(void);

static inline int mlx5_cmd_exec_check_status(struct mlx5_core_dev *dev, u32 *in,
						int in_size, u32 *out,
						int out_size)
{
	int err;
	err = mlx5_cmd_exec(dev, in, in_size, out, out_size);

	if (err) {
		return err;
	}

	err =  mlx5_cmd_status_to_err((struct mlx5_outbox_hdr *)out);
	return err;
}

int mlx5_rename_eq(struct mlx5_core_dev *dev, int eq_ix, char *name);

#endif /* __MLX5_CORE_H__ */
