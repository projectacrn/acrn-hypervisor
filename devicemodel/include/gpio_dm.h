/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _GPIO_DM_H_
#define _GPIO_DM_H_

/*
 * GPIO PIO register definition
 *
 * +---------------+----------+------+-----------+-------+
 * | Configuration | Reserved | Mode | Direction | value |
 * |      16b      |    13b   |  1b  |    1b     |  1b   |
 * +---------------+----------+------+-----------+-------+
 */

#define PIO_GPIO_VALUE_MASK	0x1
#define PIO_GPIO_DIR_OFFSET	1
#define PIO_GPIO_DIR_MASK	(0x1 << PIO_GPIO_DIR_OFFSET)
#define PIO_GPIO_MODE_OFFSET	2
#define PIO_GPIO_MODE_MASK	(0x1 << PIO_GPIO_MODE_OFFSET)
#define PIO_GPIO_CONFIG_OFFSET	16
#define PIO_GPIO_CONFIG_MASK	(0xff << PIO_GPIO_CONFIG_OFFSET)

/* PIO GPIO control method support */
#define PIO_GPIO_CM_GET	"GPCG"
#define PIO_GPIO_CM_SET	"GPCS"

/* PIO GPIO operations support */
#define PIO_GPIO_SET_VALUE(number, value) \
	PIO_GPIO_CM_SET"("#number","#value")"

#define PIO_GPIO_GET_VALUE(number) \
	PIO_GPIO_CM_GET"("#number")"
#endif
