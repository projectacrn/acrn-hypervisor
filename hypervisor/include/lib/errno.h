/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ERRNO_H
#define ERRNO_H

/** Indicates that operation not permitted. */
#define EPERM		1
/** Indicates that there is IO error. */
#define EIO		5
/** Indicates that not enough memory. */
#define ENOMEM		12
/** Indicates Permission denied */
#define EACCES		13
/** Indicates there is fault. */
#define EFAULT		14
/** Indicates that target is busy. */
#define EBUSY		16
/** Indicates that no such dev. */
#define ENODEV		19
/** Indicates that argument is not valid. */
#define EINVAL		22
/** Indicates that timeout occurs. */
#define ETIMEDOUT	110

#endif /* ERRNO_H */
