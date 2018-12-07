/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       mptable.h
 *
 *   DESCRIPTION
 *
 *       This file defines API and extern variable for VM mptable info
 *
 ************************************************************************/
/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
#ifndef MPTABLE_H
#define MPTABLE_H

struct mptable_info;

extern struct mptable_info mptable_vm1;
extern struct mptable_info mptable_vm2;

int32_t mptable_build(struct acrn_vm *vm);

#endif /* MPTABLE_H */
