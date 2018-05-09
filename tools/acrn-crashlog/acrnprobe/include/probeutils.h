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

#ifndef __PROBEUTILS_H__
#define __PROBEUTILS_H__

enum e_dir_mode {
	MODE_CRASH = 0,
	MODE_STATS,
	MODE_VMEVENT,
};

int get_uptime_string(char newuptime[24], int *hours);
int get_current_time_long(char buf[32]);
unsigned long long get_uptime(void);
char *generate_event_id(char *seed1, char *seed2);
char *generate_eventid256(char *seed);
void generate_crashfile(char *dir, char *event, char *hashkey,
			char *type, char *data0,
			char *data1, char *data2);
char *generate_log_dir(enum e_dir_mode mode, char *hashkey);

#endif
