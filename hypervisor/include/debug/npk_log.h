/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NPK_LOG_H
#define NPK_LOG_H

#define HV_NPK_LOG_REF_SHIFT  2U
#define HV_NPK_LOG_REF_MASK   ((1U << HV_NPK_LOG_REF_SHIFT) - 1U)

#define HV_NPK_LOG_MAX 1024U
#define HV_NPK_LOG_HDR 0x01000242U

enum {
	HV_NPK_LOG_CMD_INVALID = 0U,
	HV_NPK_LOG_CMD_CONF,
	HV_NPK_LOG_CMD_ENABLE,
	HV_NPK_LOG_CMD_DISABLE,
	HV_NPK_LOG_CMD_QUERY,
};

#define	HV_NPK_LOG_RES_INVALID	0x0U
#define	HV_NPK_LOG_RES_OK	0x1U
#define	HV_NPK_LOG_RES_KO	0x2U
#define	HV_NPK_LOG_RES_ENABLED	0x3U
#define	HV_NPK_LOG_RES_DISABLED	0x4U

struct hv_npk_log_param;

struct npk_chan {
	uint64_t Dn;
	uint64_t DnM;
	uint64_t DnTS;
	uint64_t DnMTS;
	uint64_t USER;
	uint64_t USER_TS;
	uint32_t FLAG;
	uint32_t FLAG_TS;
	uint32_t MERR;
	uint32_t unused;
} __packed;

#ifdef HV_DEBUG
void npk_log_setup(struct hv_npk_log_param *param);
void npk_log_write(const char *buf, size_t len);
#else
static inline void npk_log_setup(__unused struct hv_npk_log_param *param)
{}
static inline void npk_log_write(__unused const char *buf, __unused size_t len)
{}
#endif  /* HV_DEBUG */

#endif /* NPK_LOG_H */
