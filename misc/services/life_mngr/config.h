/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_
/**
 * @brief Number of seconds we're willing to retry to send shutdown request to a guest.
 * If this is 0, then there is no retry (use with caution, as guests might miss shutdown
 * command from service VM). The default value is 3.
 */
#define VM_SHUTDOWN_RETRY_TIMES 5
/**
 * @brief Number of seconds we're willing to wait for all guests to shutdown. If this is 0,
 * then there is no time out (use with caution, as guests might not respond to a shutdown
 * request). The default value is 300 seconds (5 minutes).
 */
#define SHUTDOWN_TIMEOUT 300

#define LIFE_MNGR_CONFIG_PATH "/etc/life_mngr/life_mngr.conf"
#define LIFE_MNGR_CONFIG_FOLDER "/etc/life_mngr"
#define MAX_FILE_LINE_LEN 120
#define VM_TYPE "VM_TYPE"
#define VM_NAME "VM_NAME"
#define DEV_NAME "DEV_NAME"
#define ALLOW_TRIGGER_S5 "ALLOW_TRIGGER_S5"
#define ALLOW_TRIGGER_SYSREBOOT "ALLOW_TRIGGER_SYSREBOOT"
#define MAX_CONFIG_VALUE_LEN 128

#define CHK_CREAT 1             /* create a directory, if it does not exist */
#define CHK_ONLY  0             /* check if the directory exist only */

struct life_mngr_config {
	char vm_type[MAX_CONFIG_VALUE_LEN];
	char vm_name[MAX_CONFIG_VALUE_LEN];
	char dev_names[MAX_CONFIG_VALUE_LEN];
	char allow_trigger_s5[MAX_CONFIG_VALUE_LEN];
	char allow_trigger_sysreboot[MAX_CONFIG_VALUE_LEN];
};
extern struct life_mngr_config life_conf;

/**
 * @brief Get the name of the device which is allowed to trigger system shutdown
 */
static inline char *get_allow_s5_config(struct life_mngr_config *conf)
{
	return conf->allow_trigger_s5;
}
/**
 * @brief Get the name of the device which is allowed to trigger system reboot
 */
static inline char *get_allow_sysreboot_config(struct life_mngr_config *conf)
{
	return conf->allow_trigger_sysreboot;
}
/**
 * @brief Load configuration item from config file
 *
 * @param conf_path config file path
 * @return true Load configuration items successfully
 * @return false fail to load configuration items
 */
bool load_config(char *conf_path);
/**
 * @brief Check folder exist or not, if not, create the folder
 *
 * @param path the folder path
 * @param flags the folder attribute
 */
int check_dir(const char *path, int flags);
#endif
