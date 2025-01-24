/* ***************************************************************************
 *
 * Copyright (c) 2019-2022 Samsung Electronics All Rights Reserved.
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

#include <string.h>
#include <time.h>
#include "event_groups.h"
#include "wifi.h"
#include "wifi_types.h"
#include "bk_wifi_types.h"
#include "netif_types.h"
#include <components/system.h>
#include <components/event.h>
#include "iot_debug.h"
#include "iot_bsp_wifi.h"
#include "iot_os_util.h"
#include "iot_util.h"
#include "lwip/apps/sntp.h"
#include "lwip/inet.h"

#define TAG "example"

const int WIFI_INIT_BIT = 1 << 0;
const int WIFI_STA_CONNECT_BIT = 1 << 1;
const int WIFI_STA_DISCONNECT_BIT = 1 << 2;
const int WIFI_AP_CONNECT_BIT = 1 << 3;
const int WIFI_AP_DISCONNECT_BIT = 1 << 4;
const int WIFI_SCAN_DONE_BIT = 1 << 5;

const int WIFI_EVENT_BIT_ALL = WIFI_INIT_BIT | WIFI_STA_CONNECT_BIT | WIFI_STA_DISCONNECT_BIT | WIFI_AP_CONNECT_BIT | WIFI_AP_DISCONNECT_BIT | WIFI_SCAN_DONE_BIT;

static int WIFI_INITIALIZED = false;
static EventGroupHandle_t wifi_event_group;
static iot_bsp_wifi_event_cb_t wifi_event_cb;
static bool s_wifi_connect_timeout = false;
static iot_error_t s_latest_disconnect_reason;

typedef enum
{
	EVENT_WIFI_INIT_DONE /**< WiFi initialization done event */
} wifi_event_t;

static void _initialize_sntp(void)
{
	IOT_INFO("Initializing SNTP");
	if (sntp_enabled())
	{
		IOT_INFO("SNTP is already working, STOP it first");
		sntp_stop();
	}

	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_setservername(1, "1.kr.pool.ntp.org");
	sntp_setservername(2, "1.asia.pool.ntp.org");
	sntp_setservername(3, "us.pool.ntp.org");
	sntp_setservername(4, "1.cn.pool.ntp.org");
	sntp_setservername(5, "1.hk.pool.ntp.org");
	sntp_setservername(6, "europe.pool.ntp.org");
	sntp_setservername(7, "time1.google.com");

	sntp_init();
}

static void _obtain_time(void)
{
	time_t now = 0;
	struct tm timeinfo = {0};
	int retry = 0;
	const int retry_count = 10;

	_initialize_sntp();

	while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
	{
		IOT_INFO("Waiting for system time to be set... (%d/%d)", retry, retry_count);
		IOT_DUMP(IOT_DEBUG_LEVEL_DEBUG, IOT_DUMP_BSP_WIFI_SNTP_FAIL, retry, retry_count);
		IOT_DELAY(2000);
		time(&now);
		localtime_r(&now, &timeinfo);
	}

	sntp_stop();

	if (retry < 10)
	{
		IOT_INFO("[WIFI] system time updated by %ld", now);
		IOT_DUMP(IOT_DEBUG_LEVEL_DEBUG, IOT_DUMP_BSP_WIFI_SNTP_SUCCESS, now, retry);
	}
}

static void event_cb_wifi_event(void *arg, event_module_t event_module, int event_id, void *event_data)
{
	wifi_event_sta_disconnected_t *sta_disconnected;
	wifi_event_sta_connected_t *sta_connected;
	wifi_event_ap_disconnected_t *ap_disconnected;
	wifi_event_ap_connected_t *ap_connected;

	switch (event_id)
	{
	case EVENT_WIFI_INIT_DONE:
		IOT_INFO("Wifi init done");
		xEventGroupSetBits(wifi_event_group, WIFI_INIT_BIT);
		break;
	case EVENT_WIFI_SCAN_DONE:
		IOT_INFO("Complete scanning");
		xEventGroupSetBits(wifi_event_group, WIFI_SCAN_DONE_BIT);
		break;
	case EVENT_WIFI_STA_CONNECTED:
		sta_connected = (wifi_event_sta_connected_t *)event_data;
		BK_LOGI(TAG, "STA connected to %s\n", sta_connected->ssid);
		xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECT_BIT);
		break;
	case EVENT_WIFI_STA_DISCONNECTED:
		sta_disconnected = (wifi_event_sta_disconnected_t *)event_data;
		BK_LOGI(TAG, "STA disconnected, reason(%d)\n", sta_disconnected->disconnect_reason);
		xEventGroupSetBits(wifi_event_group, WIFI_STA_DISCONNECT_BIT);
		break;
	case EVENT_WIFI_AP_CONNECTED:
		ap_connected = (wifi_event_ap_connected_t *)event_data;
		BK_LOGI(TAG, BK_MAC_FORMAT " connected to AP\n", BK_MAC_STR(ap_connected->mac));
		xEventGroupSetBits(wifi_event_group, WIFI_AP_CONNECT_BIT);
		break;
	case EVENT_WIFI_AP_DISCONNECTED:
		ap_disconnected = (wifi_event_ap_disconnected_t *)event_data;
		BK_LOGI(TAG, BK_MAC_FORMAT " disconnected from AP\n", BK_MAC_STR(ap_disconnected->mac));
		xEventGroupSetBits(wifi_event_group, WIFI_AP_DISCONNECT_BIT);
		break;
	default:
		BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
		break;
	}
}

static void event_cb_netif_event(void *arg, event_module_t event_module, int event_id, void *event_data)
{
	netif_event_got_ip4_t *got_ip;

	switch (event_id)
	{
	case EVENT_NETIF_GOT_IP4:
		got_ip = (netif_event_got_ip4_t *)event_data;
		BK_LOGI(TAG, "%s got ip\n", got_ip->netif_if == NETIF_IF_STA ? "STA" : "unknown netif");
		break;
	default:
		BK_LOGI(TAG, "rx event <%d %d>\n", event_module, event_id);
		break;
	}
}

iot_error_t iot_bsp_wifi_init()
{
	bk_err_t bk_ret;
	EventBits_t uxBits = 0;

	IOT_INFO("[bk7236] iot_bsp_wifi_init");

	if (WIFI_INITIALIZED)
		return IOT_ERROR_NONE;

	wifi_event_group = xEventGroupCreate();
	bk_ret = bk_event_register_cb(EVENT_MOD_WIFI, EVENT_ID_ALL, event_cb_wifi_event, NULL);
	if (bk_ret != BK_OK)
	{
		IOT_ERROR("Failed to register WiFi event callback");
		return IOT_ERROR_INIT_FAIL;
	}

	wifi_init_config_t wifi_config = WIFI_DEFAULT_INIT_CONFIG();

	if (bk_event_init(&wifi_config) != BK_OK)
	{
		return IOT_ERROR_INIT_FAIL;
	}

	if (bk_netif_init(&wifi_config) != BK_OK)
	{
		return IOT_ERROR_INIT_FAIL;
	}

	if (bk_wifi_init(&wifi_config) != BK_OK)
	{
		return IOT_ERROR_INIT_FAIL;
	}

	uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_INIT_BIT,
								 true, false, IOT_WIFI_CMD_TIMEOUT);

	if (uxBits & WIFI_INIT_BIT)
	{
		WIFI_INITIALIZED = true;
		IOT_INFO("[bk7236] iot_bsp_wifi_init done");
		IOT_DUMP(IOT_DEBUG_LEVEL_DEBUG, IOT_DUMP_BSP_WIFI_INIT_SUCCESS, 0, 0);
		return IOT_ERROR_NONE;
	}
	return IOT_ERROR_INIT_FAIL;
}

iot_error_t iot_bsp_wifi_set_mode(iot_wifi_conf *conf)
{
	EventBits_t uxBits = 0;

	IOT_INFO("iot_bsp_wifi_set_mode = %d", conf->mode);
	IOT_DUMP(IOT_DEBUG_LEVEL_DEBUG, IOT_DUMP_BSP_WIFI_SETMODE, conf->mode, 0);

	switch (conf->mode)
	{
	case IOT_WIFI_MODE_OFF:
		uxBits = xEventGroupGetBits(wifi_event_group);
		if (uxBits & (WIFI_STA_CONNECT_BIT | WIFI_AP_CONNECT_BIT))
		{
			IOT_INFO("Disconnecting from AP/STA");
			bk_wifi_sta_disconnect();
			bk_wifi_ap_stop();

			uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_STA_DISCONNECT_BIT | WIFI_AP_DISCONNECT_BIT,
										 true, true, IOT_WIFI_CMD_TIMEOUT);
			if ((uxBits & WIFI_STA_DISCONNECT_BIT) && (uxBits & WIFI_AP_DISCONNECT_BIT))
			{
				IOT_INFO("STA and AP disconnected");
			}
			else
			{
				IOT_ERROR("WIFI_STA_DISCONNECT_BIT or WIFI_AP_DISCONNECTED_BIT event Timeout");
				IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_TIMEOUT, conf->mode, __LINE__);
				return IOT_ERROR_CONN_OPERATE_FAIL;
			}
		}
		else
		{
			IOT_INFO("No active connection, turning off WiFi");
		}
		break;
	case IOT_WIFI_MODE_SCAN:
		uxBits = xEventGroupGetBits(wifi_event_group);

		if (uxBits & WIFI_STA_CONNECT_BIT)
		{
			IOT_INFO("Disconnecting from AP");
			bk_wifi_sta_disconnect();

			uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_STA_DISCONNECT_BIT,
										 true, false, IOT_WIFI_CMD_TIMEOUT);

			if (uxBits & WIFI_STA_DISCONNECT_BIT)
			{
				IOT_INFO("STA disconnected");
			}
			else
			{
				IOT_ERROR("WIFI_STA_DISCONNECT_BIT event Timeout");
				IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_TIMEOUT, conf->mode, __LINE__);
				return IOT_ERROR_CONN_OPERATE_FAIL;
			}
		}

		if (bk_wifi_scan() == BK_OK)
		{
			IOT_INFO("WiFi scan successful");
		}
		else
		{
			IOT_ERROR("WiFi scan failed");
			return IOT_ERROR_CONN_OPERATE_FAIL;
		}
		break;
	case IOT_WIFI_MODE_STATION:
		bk_wifi_sta_set_config(conf->ssid, conf->password, conf->bssid);

		bk_wifi_sta_start();

		uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECT_BIT,
									 true, false, IOT_WIFI_CMD_TIMEOUT);

		if (uxBits & WIFI_STA_CONNECT_BIT)
		{
			IOT_INFO("AP Connected");
			IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_CONNECT_SUCCESS, 0, 0);
			bk_wifi_sta_set_system_time();
		}
		else
		{
			IOT_ERROR("WIFI_STA_CONNECT_BIT event Timeout");
			IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_CONNECT_FAIL, IOT_WIFI_CMD_TIMEOUT,
					 s_latest_disconnect_reason);
			s_wifi_connect_timeout = true;
			return s_latest_disconnect_reason;
		}
		break;
	case IOT_WIFI_MODE_SOFTAP:
		bk_wifi_ap_set_config(conf->ssid, conf->password, conf->max_conn, conf->channel);
		bk_wifi_ap_start();
		uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_AP_CONNECT_BIT,
									 true, false, IOT_WIFI_CMD_TIMEOUT);
		if (uxBits & WIFI_AP_CONNECT_BIT)
		{
			IOT_INFO("SoftAP started successfully");
		}
		else
		{
			IOT_ERROR("WIFI_AP_STARTED_BIT event Timeout");
			IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_TIMEOUT, conf->mode, __LINE__);
			return IOT_ERROR_CONN_OPERATE_FAIL;
		}
		break;
	default:
		IOT_ERROR("bk7236 cannot support this mode = %d", conf->mode);
		IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_ERROR, conf->mode, __LINE__);
		return IOT_ERROR_CONN_OPERATE_FAIL;
	}
	return IOT_ERROR_NONE;
}

uint16_t iot_bsp_wifi_get_scan_result(iot_wifi_scan_result_t *scan_result)
{
	uint16_t ap_num = 0;
	uint16_t i;
	ScanResult_adv result;
	bk_err_t ret;
	EventBits_t uxBits = 0;

	memset(&result, 0x0, sizeof(result));

	wifi_scan_config_t config = {0};
	bk_wifi_scan_start(&config);

	uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_SCAN_DONE_BIT,
								 true, false, IOT_WIFI_CMD_TIMEOUT);

	if (!(uxBits & WIFI_SCAN_DONE_BIT))
	{
		IOT_INFO("Scan timeout");
		return 0;
	}

	ret = bk_wifi_scan_get_result(&result);
	if (ret == BK_OK)
	{
		ap_num = result.ApNum;
		ap_num = (ap_num > IOT_WIFI_MAX_SCAN_RESULT) ? IOT_WIFI_MAX_SCAN_RESULT : ap_num;

		if (ap_num > 0)
		{
			memset(scan_result, 0x0, (IOT_WIFI_MAX_SCAN_RESULT * sizeof(iot_wifi_scan_result_t)));

			for (i = 0; i < ap_num; i++)
			{
				iot_wifi_auth_mode_t conv_auth_mode;

				switch (result.ApList[i].security)
				{
				case BK_SECURITY_TYPE_WPA3_WPA2_MIXED:
				case BK_SECURITY_TYPE_WPA3_SAE:
					conv_auth_mode = IOT_WIFI_AUTH_WPA3_PERSONAL;
					break;
				default:
					conv_auth_mode = result.ApList[i].security;
					break;
				}

				strncpy(scan_result[i].ssid, result.ApList[i].ssid, WIFI_SSID_STR_LEN);
				memcpy(scan_result[i].bssid, result.ApList[i].bssid, IOT_WIFI_MAX_BSSID_LEN);
				scan_result[i].rssi = result.ApList[i].ApPower;
				scan_result[i].freq = iot_util_convert_channel_freq(result.ApList[i].channel);
				scan_result[i].authmode = conv_auth_mode;

				IOT_DEBUG("BK ssid=%s, mac=%02X:%02X:%02X:%02X:%02X:%02X, rssi=%d, freq=%d, authmode=%d channel=%d",
						  scan_result[i].ssid,
						  scan_result[i].bssid[0], scan_result[i].bssid[1], scan_result[i].bssid[2],
						  scan_result[i].bssid[3], scan_result[i].bssid[4], scan_result[i].bssid[5],
						  scan_result[i].rssi, scan_result[i].freq, scan_result[i].authmode,
						  result.ApList[i].channel);
			}
		}
	}
	else
	{
		IOT_INFO("failed to get Wi-Fi scan result");
		IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_ERROR, 0, __LINE__);
		ap_num = 0;
	}

	return ap_num;
}

iot_error_t iot_bsp_wifi_get_mac(struct iot_mac *wifi_mac)
{
	bk_err_t bk_ret;
	bk_ret = bk_get_mac(wifi_mac->addr, MAC_TYPE_STA);
	if (bk_ret != BK_OK)
	{
		IOT_ERROR("failed to read wifi mac address : %d", bk_ret);
		IOT_DUMP(IOT_DEBUG_LEVEL_ERROR, IOT_DUMP_BSP_WIFI_ERROR, 0, __LINE__);
		return IOT_ERROR_CONN_OPERATE_FAIL;
	}

	return IOT_ERROR_NONE;
}

iot_wifi_freq_t iot_bsp_wifi_get_freq(void)
{
	return IOT_WIFI_FREQ_2_4G_ONLY;
}

iot_error_t iot_bsp_wifi_register_event_cb(iot_bsp_wifi_event_cb_t cb)
{
	if (cb == NULL)
	{
		return IOT_ERROR_INVALID_ARGS;
	}

	wifi_event_cb = cb;
	return IOT_ERROR_NONE;
}

void iot_bsp_wifi_clear_event_cb(void)
{
	wifi_event_cb = NULL;
}

iot_wifi_auth_mode_bits_t iot_bsp_wifi_get_auth_mode(void)
{
	iot_wifi_auth_mode_bits_t supported_mode_bits = IOT_WIFI_AUTH_MODE_BIT_ALL;
	supported_mode_bits ^= IOT_WIFI_AUTH_MODE_BIT(IOT_WIFI_AUTH_WPA2_ENTERPRISE);

	return supported_mode_bits;
}
