/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PROPERTY_H__
#define __PROPERTY_H__
#include "load_conf.h"

#define VERSION_SIZE		256
/* UUID_SIZE contains the UUID number, dashes and some buffer*/
#define UUID_SIZE		48
/* General BUILD_VERSION like 23690 */
#define BUILD_VERSION_SIZE	16

char guuid[UUID_SIZE];
char gbuildversion[BUILD_VERSION_SIZE];

int init_properties(struct sender_t *sender);
int swupdated(struct sender_t *sender);

#endif
