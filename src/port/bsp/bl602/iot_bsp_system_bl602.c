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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "task.h"
#include "hal_sys.h"
#include "iot_bsp_system.h"
#include "iot_debug.h"
#include <hosal_rtc.h>

const char *iot_bsp_get_bsp_name()
{
	return "bl602";
}

const char *iot_bsp_get_bsp_version_string()
{
	setenv("CONFIG_CHIP_NAME", "BL602", 1);
	getenv("BL_SDK_VER");
	return BL_SDK_VER;
}

void iot_bl_system_reboot()
{
	// Disable scheduler on this core.
	vTaskSuspendAll();
	hal_sys_reset();
}

void iot_bl_system_poweroff()
{
	iot_bl_system_reboot(); // no poweroff feature.
}

iot_error_t iot_bl_system_get_time_in_sec(char *buf, unsigned int buf_len)
{
	IOT_WARN_CHECK(buf == NULL, IOT_ERROR_INVALID_ARGS, "buffer for time is NULL");

	int ret = -1;
	/*init rtc DEC format*/
	hosal_rtc_dev_t rtc;
	rtc.port = 0;
	rtc.config.format = HOSAL_RTC_FORMAT_DEC;
	hosal_rtc_init(&rtc);

	hosal_rtc_time_t time_buf;
	/* clear time buf*/
	memset(&time_buf, 0, sizeof(hosal_rtc_time_t));

	/* get rtc time */
	ret = hosal_rtc_get_time(&rtc, &time_buf);
	if (0 != ret)
	{
		IOT_ERROR("rtc get time error\r\n");
		return IOT_ERROR_INVALID_ARGS;
	}

	snprintf(buf, buf_len, "%02hu-%02hhu-%02hhu-%02hhu-%02hhu-%02hhu", time_buf.year, time_buf.month, time_buf.date, time_buf.hr, time_buf.min, time_buf.sec);

	return IOT_ERROR_NONE;
}

iot_error_t iot_bl_system_set_time_in_sec(const char *time_in_sec)
{
	IOT_WARN_CHECK(time_in_sec == NULL, IOT_ERROR_INVALID_ARGS, "time data is NULL");

	int ret = -1;
	/*init rtc DEC format*/
	hosal_rtc_dev_t rtc;
	rtc.port = 0;
	rtc.config.format = HOSAL_RTC_FORMAT_DEC;
	hosal_rtc_init(&rtc);

	hosal_rtc_time_t time_buf;
	/* clear time buf*/
	memset(&time_buf, 0, sizeof(hosal_rtc_time_t));

	if (6 != sscanf(time_in_sec, "%2hu-%2hhu-%2hhu-%2hhu-%2hhu-%2hhu", &time_buf.year, &time_buf.month, &time_buf.date, &time_buf.hr, &time_buf.min, &time_buf.sec))
	{
		IOT_ERROR("Parsing failure\r\n");
	}
	else
	{
		printf("%hu-%hhu-%hhu-%hhu-%hhu-%hhu\r\n", time_buf.year + 2000, time_buf.month, time_buf.date, time_buf.hr, time_buf.min, time_buf.sec);
	}

	ret = hosal_rtc_set_time(&rtc, &time_buf);
	if (0 != ret)
	{
		IOT_ERROR("rtc get time error\r\n");
		return IOT_ERROR_INVALID_ARGS;
	}

	return IOT_ERROR_NONE;
}
