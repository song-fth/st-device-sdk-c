/* ***************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <easyflash.h>
#include <stdio.h>
#include "iot_bsp_nv_data.h"
#include "iot_bsp_fs.h"
#include "iot_debug.h"

//#define STDK_NV_DATA_NAMESPACE "stdk"
//#define STDK_NV_SECTOR_SIZE	(EF_ERASE_MIN_SIZE)
#define STDK_NV_SECTOR_SIZE 512
#define MAX_NV_ITEM_CNT		19
char * nv_set[MAX_NV_ITEM_CNT] = {
	"WifiProvStatus",   // WifiProvStatus
	"IotAPSSID",		// IotAPSSID
	"IotAPPASS",		// IotAPPASS
	"IotAPBSSID",		// IotAPBSSID
	"IotAPAuthType",	// IotAPAuthType
	"CloudProvStatus",	// CloudProvStatus
	"ServerURL",		// ServerURL
	"ServerPort",		// ServerPort
	"Label",		    // Label
	"DeviceID",		    // DeviceID
	"MiscInfo",		    // MiscInfo
	"PrivateKey",		// PrivateKey
	"PublicKey",		// PublicKey
	"PKType",		    // PKType
	"RootCert",		    // RootCert
	"SubCert",		    // SubCert
	"DeviceCert",	    // DeviceCert
	"ClaimID",		    // ClaimID
	"SerialNum",	    // SerialNum
};

static void nv_storage_init(void)
{
	uint8_t data = 0xFF;
	char tmp[32] = {0};
	for (int i = 0; i < MAX_NV_ITEM_CNT; i++) {
		memset(tmp, 0, sizeof(tmp));
		strncpy(tmp, nv_set[i], sizeof(tmp));
		ef_set_env_blob(tmp, &data, STDK_NV_SECTOR_SIZE);
	}
}

iot_error_t iot_bsp_fs_init()
{
	int ret = -1;
	ret = easyflash_init();
	if ( IOT_ERROR_NONE != ret) {
		IOT_DEBUG("fs init fail");
		return IOT_ERROR_INIT_FAIL;
	}
	nv_storage_init();
	return IOT_ERROR_NONE;
}

iot_error_t iot_bsp_fs_deinit()
{
	return IOT_ERROR_NONE;//no fs deinitfeature.
}

iot_error_t iot_bsp_fs_open(const char* filename, iot_bsp_fs_open_mode_t mode, iot_bsp_fs_handle_t* handle)
{
	iot_bsp_fs_open_mode_t flag = mode;

	if (NULL == filename) {
		IOT_DEBUG("filename is NULL,open failed");
		return IOT_ERROR_INVALID_ARGS;
	}
	snprintf(handle->filename, sizeof(handle->filename), "%s", filename);
	return IOT_ERROR_NONE;
}

iot_error_t iot_bsp_fs_close(iot_bsp_fs_handle_t handle)
{
	return IOT_ERROR_NONE;
}

iot_error_t iot_bsp_fs_read(iot_bsp_fs_handle_t handle, char* buffer, size_t *length)
{
	int ret;
	size_t read_len = 0;
	if (NULL == buffer) {
		IOT_DEBUG("buffer is NULL,read failed");
		return IOT_ERROR_INVALID_ARGS;
	}
	
	ret = ef_get_env_blob(handle.filename, buffer, *length, &read_len);
	if (0 > ret) {
		IOT_DEBUG("data read failed");
		return IOT_ERROR_FS_READ_FAIL;
	}
	return IOT_ERROR_NONE;
}

iot_error_t iot_bsp_fs_write(iot_bsp_fs_handle_t handle, const char* data, unsigned int length)
{
	int ret;
	size_t read_len = 0;
	ret = ef_set_env_blob(handle.filename, data, length);
	IOT_DEBUG_CHECK(ret < 0, IOT_ERROR_FS_WRITE_FAIL, "data write failed");
	return IOT_ERROR_NONE;
}

iot_error_t iot_bsp_fs_remove(const char* filename)
{
	int ret;
        if (NULL == filename) {
		IOT_DEBUG("filename is NULL,remove failed");
		return IOT_ERROR_INVALID_ARGS;
	}
	ret = ef_del_env(filename);
	if (0 != ret) {
		IOT_DEBUG("remove file failed");
		return IOT_ERROR_FS_REMOVE_FAIL;
	}
	return IOT_ERROR_NONE;
}



