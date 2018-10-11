/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static int32_t npk_log_setup_ref;
static bool npk_log_enabled;
static uint64_t base;

static inline int npk_write(const char *value, void *addr, size_t sz)
{
	int ret = -1;

	if (sz >= 8U) {
		mmio_write64(*(uint64_t *)value, addr);
		ret = 8;
	} else if (sz >= 4U) {
		mmio_write32(*(uint32_t *)value, addr);
		ret = 4;
	} else if (sz >= 2U) {
		mmio_write16(*(uint16_t *)value, addr);
		ret = 2;
	} else if (sz >= 1U) {
		mmio_write8(*(uint8_t *)value, addr);
		ret = 1;
	}

	return ret;
}

void npk_log_setup(struct hv_npk_log_param *param)
{
	int i;

	pr_info("HV_NPK_LOG: cmd %d param 0x%llx\n", param->cmd,
			param->mmio_addr);

	param->res = HV_NPK_LOG_RES_KO;
	if (atomic_inc_return(&npk_log_setup_ref) > 1) {
		goto out;
	}

	switch (param->cmd) {
	case HV_NPK_LOG_CMD_CONF:
		if ((param->mmio_addr != 0UL) || (param->loglevel != 0xffffU)) {
			param->res = HV_NPK_LOG_RES_OK;
		}
		/* falls through */
	case HV_NPK_LOG_CMD_ENABLE:
		if (param->mmio_addr != 0UL) {
			base = param->mmio_addr;
		}
		if (param->loglevel != 0xffffU) {
			npk_loglevel = param->loglevel;
		}
		if ((base != 0UL) && (param->cmd == HV_NPK_LOG_CMD_ENABLE)) {
			if (!npk_log_enabled) {
				for (i = 0; i < phys_cpu_num; i++)
					per_cpu(npk_log_ref, i) = 0;
			}
			param->res = HV_NPK_LOG_RES_OK;
			npk_log_enabled = 1;
		}
		break;
	case HV_NPK_LOG_CMD_DISABLE:
		npk_log_enabled = 0;
		param->res = HV_NPK_LOG_RES_OK;
		break;
	case HV_NPK_LOG_CMD_QUERY:
		param->res = npk_log_enabled ? HV_NPK_LOG_RES_ENABLED :
			HV_NPK_LOG_RES_DISABLED;
		param->loglevel = npk_loglevel;
		param->mmio_addr = base;
		break;
	default:
		pr_err("HV_NPK_LOG: unknown cmd (%d)\n", param->cmd);
		break;
	}

out:
	pr_info("HV_NPK_LOG: result %d\n", param->res);
	atomic_dec32((uint32_t *)&npk_log_setup_ref);
}

void npk_log_write(const char *buf, size_t buf_len)
{
	uint32_t cpu_id = get_cpu_id();
	struct npk_chan *channel = (struct npk_chan *)base;
	const char *p = buf;
	int sz;
	uint32_t ref;
	uint16_t len;

	if (!npk_log_enabled || (channel == NULL)) {
		return;
	}

	/* calculate the channel offset based on cpu_id and npk_log_ref */
	ref = (atomic_inc_return((int32_t *)&per_cpu(npk_log_ref, cpu_id)) - 1)
		& HV_NPK_LOG_REF_MASK;
	channel += (cpu_id << HV_NPK_LOG_REF_SHIFT) + ref;
	len = (uint16_t)(min(buf_len, HV_NPK_LOG_MAX));
	mmio_write32(HV_NPK_LOG_HDR, &(channel->DnTS));
	mmio_write16(len, &(channel->Dn));

	for (sz = 0; sz >= 0; p += sz)
		sz = npk_write(p, &(channel->Dn), buf + len - p);

	mmio_write8(0U, &(channel->FLAG));

	atomic_dec32(&per_cpu(npk_log_ref, cpu_id));
}
