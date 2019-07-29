/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ext2fs/ext2fs.h>

int loopdev_num_get_free(void);
int loopdev_set_img_par(const char *loopdev, const char *img_path,
			const char *parname);
int loopdev_check_parname(const char *loopdev, const char *parname);
int e2fs_dump_file_by_fpath(ext2_filsys fs, const char *in_fp,
			const char *out_fp);
int e2fs_read_file_by_fpath(ext2_filsys fs, const char *in_fp,
			 void **out_data, unsigned long *size);
int e2fs_dump_dir_by_dpath(ext2_filsys fs, const char *in_dp,
			const char *out_dp, int *count);
int e2fs_open(const char *dev, ext2_filsys *outfs);
void e2fs_close(ext2_filsys fs);
