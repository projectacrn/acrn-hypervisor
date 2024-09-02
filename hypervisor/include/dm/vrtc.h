/*
 * Copyright (C) 2024 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VRTC_H
#define VRTC_H

/**
 * @defgroup vp-dm_vperipheral vp-dm.vperipheral
 * @ingroup vp-dm
 * @brief Implementation of virtual peripheral devices in hypervisor.
 *
 * This module implements the virtualization of all peripheral devices in hypervisor. The virtual device initial
 * function is usually called by the VM initialization function and registers their port IO and memory IO access
 * functions. So when a guest VM accesses its peripheral device by port IO or memory IO, it would cause VM exit and then
 * call their registered functions.
 * @{
 */

/**
 * @file
 * @brief Definitions for the virtual RTC device.
 *
 * This file defines types and data structure for the virtual RTC device.
 */

typedef int32_t time_t;

/* Register layout of the RTC */
struct rtcdev {
	uint8_t	sec;
	uint8_t	alarm_sec;
	uint8_t	min;
	uint8_t	alarm_min;
	uint8_t	hour;
	uint8_t	alarm_hour;
	uint8_t	day_of_week;
	uint8_t	day_of_month;
	uint8_t	month;
	uint8_t	year;
	uint8_t	reg_a;
	uint8_t	reg_b;
	uint8_t	reg_c;
	uint8_t	reg_d;
	uint8_t	res[36];
	uint8_t	century;
};

/**
 * @brief Data structure to illustrate a virtual RTC device.
 *
 * This structure contains the information of a virtual RTC device.
 *
 * @consistency self.vm->vrtc == self
 * @alignment N/A
 *
 * @remark N/A
 */
struct acrn_vrtc {
	struct acrn_vm	*vm; /**< Pointer to the VM that owns the virtual RTC device. */
	/**
	 * @brief The RTC register to read or write.
	 *
	 * To access RTC registers, the guest writes the register index to the RTC address port and then reads/writes
	 * the register value from/to the RTC data port. This field is used to store the register index.
	 */
	uint32_t	addr;
	time_t		base_rtctime; /**< Base time calculated from physical RTC register. */
	time_t		offset_rtctime; /**< RTC offset against base time. */
	time_t		last_rtctime; /**< Last RTC time, to keep monotonicity. */
	uint64_t	base_tsc; /**< Base TSC value. */
	struct rtcdev	rtcdev; /**< Register layout of RTC. */
};

/**
 * @}
 */
#endif /* VRTC_H */