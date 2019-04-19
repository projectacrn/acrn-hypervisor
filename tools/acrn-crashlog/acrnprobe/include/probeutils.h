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

#define UPTIME_SIZE 24
#define LONG_TIME_SIZE 32
#define SHORT_KEY_LENGTH 20
#define LONG_KEY_LENGTH 32

enum e_dir_mode {
	MODE_CRASH = 0,
	MODE_STATS,
	MODE_VMEVENT,
};

enum key_type {
	KEY_SHORT = 0,
	KEY_LONG,
};

int get_uptime_string(char newuptime[24], int *hours);
int get_current_time_long(char buf[32]);
unsigned long long get_uptime(void);
char *generate_event_id(const char *seed1, size_t slen1, const char *seed2,
			size_t slen2, enum key_type type);
void generate_crashfile(const char *dir, const char *event, size_t elen,
			const char *hashkey, size_t hlen,
			const char *type, size_t tlen, const char *data0,
			size_t d0len, const char *data1, size_t d1len,
			const char *data2, size_t d2len);
char *generate_log_dir(enum e_dir_mode mode, char *hashkey, size_t *dlen);
int is_boot_id_changed(void);

#endif
