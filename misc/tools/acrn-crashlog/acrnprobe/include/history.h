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

#ifndef __HISTORY_H__
#define __HISTORY_H__

#define HISTORY_NAME		"history_event"

extern char *history_file;

int prepare_history(void);
void hist_raise_infoerror(const char *type, size_t tlen);
void hist_raise_uptime(char *lastuptime);
void hist_raise_event(const char *event, const char *type, const char *log,
			const char *lastuptime, const char *key);

#endif
