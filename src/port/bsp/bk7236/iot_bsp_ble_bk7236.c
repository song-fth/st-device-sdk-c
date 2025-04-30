/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/****************************************************************************
 *
 * This demo showcases BLE GATT server. It can send adv data, be connected by client.
 * Run the gatt_client demo, the client demo will automatically connect to the gatt_server demo.
 * Client demo will enable gatt_server's notify after connection. The two devices will then exchange
 * data.
 *
 ****************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <os/os.h>
#include <os/mem.h>

// #include <stdio.h>
// #include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <driver/aon_rtc.h>
#include "components/bluetooth/bk_ble.h"

#include "components/bluetooth/bk_dm_bluetooth.h"
#include "components/bluetooth/bk_dm_gatts.h"

#include "iot_debug.h"
#include "iot_bsp_ble.h"
#include "iot_os_util.h"
#define GATTS_TAG "BLE_ONBOARD"

#define DISPLAY_SERIAL_NUMBER_SIZE 4

#define PACKET_MAX_SIZE     31
#define MAC_ADD_COUNT       6
#define BT_MAC_LENGTH       6

#define ADV_FLAG_LEN 0x02
#define ADV_FLAG_TYPE 0x01
#define ADV_FLAG_VALUE 0x04
#define ADV_SERVICE_DATA_LEN 0x1B

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

#define CUSTOM_DATA_TYPE 0xFF
#if defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
#define CUSTOM_DATA_LEN 0x06
#define CUSTOM_TYPE 0x02
#define CUSTOM_TYPE_DATA_LEN 0x04
#else
#define CUSTOM_DATA_LEN 0x0A
#define CUSTOM_TYPE 0x03
#define CUSTOM_TYPE_DATA_LEN 0x08
#endif

#define CONTROL_VERSION_ACTIVE_SCAN_REQUIRED    0x42
#define PACKET_VERSION        0x83
#define SERVICE_ID            0x0c
#define OOB_SERVICE_INFO      0x05
#define SERVICE_FEATURE       0x59
#define SETUP_AVAILABLE_NETWORK_BLE    0x04

#define BT_ADDRESS_TRANSFER 0x01

#define SCAN_RBK_FLAG_TYPE 0x09
#if defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
#define SCAN_RBK_MF_DATA_LEN       0x06
#define MAX_DEVICE_NAME_DATA_LEN    0x16
#else
#define SCAN_RBK_MF_DATA_LEN       0x0A
#define MAX_DEVICE_NAME_DATA_LEN    0x12
#endif

#define GATTS_MTU_MAX    23

static beken_semaphore_t gatt_sema = NULL;
#define GATT_SYNC_CMD_TIMEOUT_MS          3000


size_t device_onboarding_id_len;
int indication_need_confirmed;
int gatt_connected;

/* ble status */
enum bk_ble_status
{
	Bk_BLE_STATUS_UNKNOWN,
	Bk_BLE_STATUS_INIT,
	Bk_BLE_STATUS_DEINIT,
};

enum
{
	PRF_TASK_ID_GATTS = 10,
	PRF_TASK_ID_MAX,
};

static enum bk_ble_status g_ble_status = Bk_BLE_STATUS_UNKNOWN;
static uint32_t g_mtu = 0;
static uint8_t adv_config_done = 0;
static uint8_t adv_data[PACKET_MAX_SIZE];
static size_t adv_data_len;
static size_t adv_data_mac_address_offset;
static uint8_t scan_response_data[PACKET_MAX_SIZE];
static size_t scan_response_len;
static uint8_t manufacturer_id[2] = {0x75, 0x00};
static iot_bsp_ble_event_cb_t ble_event_cb;
static bool g_onboarding_complete;

CharWriteCallback CharWriteCb;

#define BLE_GATTS_ADV_INTERVAL_MIN 120
#define BLE_GATTS_ADV_INTERVAL_MAX 160
#define INVALID_HANDLE          0xFF
#define UNKNOW_ACT_IDX         0xFFU
#define DECL_PRIMARY_SERVICE_128     {0x00,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define DECL_CHARACTERISTIC_128      {0x03,0x28,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define DESC_CLIENT_CHAR_CFG_128     {0x02,0x29,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
#define BLE_ONBOARDING_CHAR_UUID {0x09, 0x0E, 0xE6, 0x80, 0x02, 0x30, 0xC7, 0xA4, 0x8E, 0x4F, 0x2D, 0xAE, 0x0E, 0x0F, 0x94, 0xBE}
#define GATTS_CHARA_PROPERTIES_UUID 			(0xEA01)
#define GATTS_SERVICE_UUID 					(0xFA00)
enum {
    GATTS_IDX_SVC,
    GATTS_IDX_CHAR_DECL,
    GATTS_IDX_CHAR_VALUE,
	GATTS_IDX_CHAR_DESC,
	GATTS_IDX_NB,
};
#define BLE_ONBOARDING_SERVICE_UUID BT_UUID_DECLARE_16(0xFD1D)
int actv_idx = 0;
ble_adv_param_t adv_param;
uint8 g_test_prf_task_id1 = 0;
static ble_err_t gatt_cmd_status = BK_ERR_BLE_SUCCESS;
static uint8_t gatt_conn_ind = INVALID_HANDLE;

// static ble_attm_desc_t gatts_service_db[GATTS_IDX_NB] = {
//     //  Service Declaration
//     [GATTS_IDX_SVC]        = {BLE_ONBOARDING_CHAR_UUID , BK_BLE_PERM_SET(RD, ENABLE), 0, 0},

//     [GATTS_IDX_CHAR_DECL]  = {DECL_CHARACTERISTIC_128,  BK_BLE_PERM_SET(RD, ENABLE), 0, 0},
//     // Characteristic Value
//     [GATTS_IDX_CHAR_VALUE] = {{GATTS_CHARA_PROPERTIES_UUID & 0xFF, GATTS_CHARA_PROPERTIES_UUID >> 8}, BK_BLE_PERM_SET(NTF, ENABLE), BK_BLE_PERM_SET(RI, ENABLE) | BK_BLE_PERM_SET(UUID_LEN, UUID_16), 128},
// 	//Client Characteristic Configuration Descriptor
// 	[GATTS_IDX_CHAR_DESC] = {DESC_CLIENT_CHAR_CFG_128, BK_BLE_PERM_SET(RD, ENABLE) | BK_BLE_PERM_SET(WRITE_REQ, ENABLE), 0, 0},
// };

static ble_attm_desc_t gatts_service_db[GATTS_IDX_NB] = {
    //  Service Declaration
    [GATTS_IDX_SVC]        = {DECL_PRIMARY_SERVICE_128 , BK_BLE_PERM_SET(RD, ENABLE), 0, 0},

    [GATTS_IDX_CHAR_DECL]  = { DECL_CHARACTERISTIC_128,  BK_BLE_PERM_SET(RD, ENABLE), 0, 0},
    // Characteristic ValueIND
    [GATTS_IDX_CHAR_VALUE] = {BLE_ONBOARDING_CHAR_UUID, BK_BLE_PERM_SET(WRITE_REQ, ENABLE)|BK_BLE_PERM_SET(IND, ENABLE), BK_BLE_PERM_SET(RI, ENABLE) | BK_BLE_PERM_SET(UUID_LEN, UUID_128), 128},
	//Client Characteristic Configuration Descriptor
	[GATTS_IDX_CHAR_DESC] = {DESC_CLIENT_CHAR_CFG_128, BK_BLE_PERM_SET(RD, ENABLE) | BK_BLE_PERM_SET(WRITE_REQ, ENABLE), 0, 0},

};

static bool iot_bsp_ble_get_onboarding_completion(void)
{
	return g_onboarding_complete;
}

void iot_bsp_ble_set_onboarding_completion(bool onboarding_complete)
{
	g_onboarding_complete = onboarding_complete;
}

static void set_advertise_mac_addr(uint8_t **mac)
{
	int i;
	int counter = adv_data_mac_address_offset;
	uint8_t *lmac = NULL;

	lmac = (uint8_t *)malloc(BT_MAC_LENGTH);
	if (!lmac) {
		IOT_INFO("failed to malloc for lmac\n");
		*mac = NULL;
		return;
	}
// bk_bluetooth_get_address(mac);
	bt_err_t err = bk_bluetooth_get_address(lmac);
	if (err != BK_OK) {
		IOT_INFO("failed to read bt mac\n");
		free(lmac);
		*mac = NULL;
		return;
	}

	for (i = 0; i < BT_MAC_LENGTH; i++) {
		adv_data[counter++] = lmac[i];
	}

	*mac = lmac;
}

void iot_create_advertise_packet(char *mnid, char *setupid, char *serial)
{
	uint8_t *mac;
	int count = 0;
	int i;
	int mnid_len = 0;
	int setupid_len = 0;

#if !defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
	char *hybrid_serial = serial;
#else
	int serial_len = 0;
	unsigned char display_serial[DISPLAY_SERIAL_NUMBER_SIZE + 1] = {0,};

	serial_len = strlen(serial);

	for (i = 0; i < DISPLAY_SERIAL_NUMBER_SIZE; i++)
	{
		display_serial[i] = serial[serial_len - DISPLAY_SERIAL_NUMBER_SIZE + i];
	}
	display_serial[DISPLAY_SERIAL_NUMBER_SIZE] = '\0';
	BK_LOGI(">> Display_Serial [%c%c%c%c] <<", display_serial[0], display_serial[1],
			display_serial[2], display_serial[3]);
#endif

	adv_data[count++] = ADV_FLAG_LEN;
	adv_data[count++] = ADV_FLAG_TYPE;
	adv_data[count++] = ADV_FLAG_VALUE;
	adv_data[count++] = ADV_SERVICE_DATA_LEN;
	adv_data[count++] = CUSTOM_DATA_TYPE;
	adv_data[count++] = manufacturer_id[0];
	adv_data[count++] = manufacturer_id[1];
	adv_data[count++] = CONTROL_VERSION_ACTIVE_SCAN_REQUIRED;
	adv_data[count++] = SERVICE_ID;
	adv_data[count++] = PACKET_VERSION;
	adv_data[count++] = OOB_SERVICE_INFO;
	adv_data[count++] = SERVICE_FEATURE;

	mnid_len = strlen(mnid);

	for (i = 0; i < mnid_len; i++)
	{
		adv_data[count++] = (uint8_t)mnid[i];
	}

	setupid_len = strlen(setupid);

	for (i = 0; i < setupid_len; i++)
	{
		adv_data[count++] = (uint8_t)setupid[i];
	}

	adv_data[count++] = SETUP_AVAILABLE_NETWORK_BLE;
	adv_data[count++] = BT_ADDRESS_TRANSFER;

	adv_data_mac_address_offset = count;
	count += MAC_ADD_COUNT;
	set_advertise_mac_addr(&mac);
IOT_INFO("1111111111111111111\n");
	adv_data[count++] = CUSTOM_DATA_LEN;
	adv_data[count++] = CUSTOM_TYPE;
	adv_data[count++] = CUSTOM_TYPE_DATA_LEN;
#if defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
	adv_data[count++] = display_serial[0];
#else
	adv_data[count++] = hybrid_serial[0];
#endif
IOT_INFO("1111111111111111111\n");
	adv_data_len = count;

	IOT_INFO("adv_data_len : %d\n",count);
	//coredump
	for (i = 0; i < count; i++)
	{
		IOT_INFO("0x%x,  ", adv_data[i]);
	}
	IOT_INFO("\n");
	IOT_INFO("1111111111111111111\n");
}


void iot_create_scan_response_packet(char *device_onboarding_id, char *serial)
{
	int count = 0;
	int i;
#if !defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
	char *hybrid_serial = serial;
#else
	int serial_len = 0;
	size_t device_onboarding_id_len = 0;
	unsigned char display_serial[DISPLAY_SERIAL_NUMBER_SIZE + 1] = { 0,};

	serial_len = strlen(serial);

	for (i = 0; i < DISPLAY_SERIAL_NUMBER_SIZE; i++) {
		display_serial[i] = serial[serial_len - DISPLAY_SERIAL_NUMBER_SIZE + i];
	}
	display_serial[DISPLAY_SERIAL_NUMBER_SIZE] = '\0';
#endif

	device_onboarding_id_len = (strlen(device_onboarding_id) > MAX_DEVICE_NAME_DATA_LEN) ? MAX_DEVICE_NAME_DATA_LEN : strlen(device_onboarding_id);

	scan_response_data[count++] = device_onboarding_id_len + 1;
	scan_response_data[count++] = SCAN_RBK_FLAG_TYPE;

	for (i = 0; i < device_onboarding_id_len; i++) {
		scan_response_data[count++] = (uint8_t ) device_onboarding_id[i];
	}

	scan_response_data[count++] = SCAN_RBK_MF_DATA_LEN;
	scan_response_data[count++] = CUSTOM_DATA_TYPE;
	scan_response_data[count++] = manufacturer_id[0];
	scan_response_data[count++] = manufacturer_id[1];

#if defined(CONFIG_STDK_IOT_CORE_EASYSETUP_X509)
	for (i = 1; i < DISPLAY_SERIAL_NUMBER_SIZE; i++) {
		scan_response_data[count++] = (uint8_t ) display_serial[i];
	}
#else
	for (i = 1; i < HYBRID_SERIAL_NUMBER_SIZE; i++) {
		scan_response_data[count++] = (uint8_t ) hybrid_serial[i];
	}
#endif

	scan_response_len = count;

	for (i = count; i < PACKET_MAX_SIZE; i++) {
		scan_response_data[count++] = 0;
	}

	IOT_INFO("\n");
	for(i = 0; i < PACKET_MAX_SIZE; i++) {
		IOT_INFO("0x%x,  ", scan_response_data[i]);
	}
	IOT_INFO("\n");
}

int iot_send_indication(uint8_t *buf, uint32_t len)
{
	struct timeval start_tv = {0,}, elasped_tv = {0,};

	bd_addr_t connect_addr;
	uint8_t conn_idx;
	bk_bluetooth_get_address(connect_addr.addr);
	conn_idx = bk_ble_find_conn_idx_from_addr(&connect_addr);

	gettimeofday(&start_tv, NULL);

	while(indication_need_confirmed && gatt_connected) {
		gettimeofday(&elasped_tv, NULL);
		if (elasped_tv.tv_sec - start_tv.tv_sec >= 5) {
			IOT_INFO("Wait confirm timeout 5s");
			return 1;
		}
	}

	if (!gatt_connected) {
		IOT_INFO(GATTS_TAG, "%s No gatt connection", __func__);
		return 1;
	}
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
        if ((i + 1) % 16 == 0) {
            IOT_INFO("\n");
        }
    }
	// indication_need_confirmed = 1;
	ble_err_t result = bk_ble_send_ind_value(conn_idx, len, buf, PRF_TASK_ID_GATTS, GATTS_IDX_CHAR_VALUE);
	IOT_INFO("send indication result : %d\t\n",result);

	// bt_err_t ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

    // if (ret != BK_OK)
    // {
    //     IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
    //     //goto error;
    // }
    // else
    // {
    //     IOT_INFO("create gatt db success\n");
    // }
	return 0;
}


static void ble_gatt_cmd_cb(ble_cmd_t cmd, ble_cmd_param_t *param)
{
    gatt_cmd_status = param->status;
	IOT_INFO("ble_gatt_cmd_cb : %d ",cmd);
    switch (cmd)
    {
        case BLE_CREATE_ADV:
        case BLE_SET_ADV_DATA:
        case BLE_SET_RSP_DATA:
        case BLE_START_ADV:
        case BLE_STOP_ADV:
        case BLE_CREATE_SCAN:
        case BLE_START_SCAN:
        case BLE_STOP_SCAN:
        case BLE_INIT_CREATE:
        case BLE_INIT_START_CONN:
        case BLE_INIT_STOP_CONN:
        case BLE_CONN_DIS_CONN:
        case BLE_CONN_UPDATE_PARAM:
        case BLE_DELETE_ADV:
        case BLE_DELETE_SCAN:
        case BLE_CONN_READ_PHY:
        case BLE_CONN_SET_PHY:
        case BLE_CONN_UPDATE_MTU:
        case BLE_SET_ADV_RANDOM_ADDR:
            break;
        default:
            break;
    }

}


static bk_ble_key_t s_ble_enc_key;
static void ble_gatts_notice_cb(ble_notice_t notice, void *param)
{
		IOT_INFO("ble_gatts_notice_cb : %d ",notice);
    switch (notice) {
    case BLE_5_STACK_OK:
        IOT_INFO("ble stack ok");
        break;
    case BLE_5_WRITE_EVENT: {
        ble_write_req_t *w_req = (ble_write_req_t *)param;
        IOT_INFO("write_cb:conn_idx:%d, prf_id:%d, att_idx:%d, len:%d, data[0]:0x%02x\r\n",
                w_req->conn_idx, w_req->prf_id, w_req->att_idx, w_req->len, w_req->value[0]);
//#if (CONFIG_BTDM_5_2)

		if (bk_ble_get_controller_stack_type() == BK_BLE_CONTROLLER_STACK_TYPE_BTDM_5_2
            && w_req->prf_id == PRF_TASK_ID_GATTS) {
			switch(w_req->att_idx)
            {
            case GATTS_IDX_CHAR_DECL:
			IOT_INFO("write notify GATTS_IDX_CHAR_DECL: %02X %02X, length: %d\n", w_req->value[0], w_req->value[1], w_req->len);
				// for(int m = 0;m<w_req->len;m++){
				// 	IOT_INFO("%02X ", w_req->value[m]);
				// }
                break;
            case GATTS_IDX_CHAR_VALUE:
				IOT_INFO("write notify GATTS_IDX_CHAR_VALUE: %02X %02X, length: %d\n", w_req->value[0], w_req->value[1], w_req->len);
				// for(int m = 0;m<w_req->len;m++){
				// 	IOT_INFO("%02X ", w_req->value[m]);

				// }	
					if (CharWriteCb) {
					CharWriteCb(w_req->value, w_req->len);
				}
                break;
			case GATTS_IDX_CHAR_DESC: {
				IOT_INFO("write notify GATTS_IDX_CHAR_DESC: %02X %02X, length: %d\n", w_req->value[0], w_req->value[1], w_req->len);
				// for(int m = 0;m<w_req->len;m++){
				// 	IOT_INFO("%02X ", w_req->value[m]);
				// }
				break;
			}
            default:
                break;
            }

        }
//#endif
		// for(int m = 0;m<w_req->len;m++){
		// 			IOT_INFO("%02X ", w_req->value[m]);
		// 		}

        break;
    }
    case BLE_5_READ_EVENT: {
        ble_read_req_t *r_req = (ble_read_req_t *)param;
        IOT_INFO("read_cb:conn_idx:%d, prf_id:%d, att_idx:%d\r\n",
                r_req->conn_idx, r_req->prf_id, r_req->att_idx);

        if (r_req->prf_id == g_test_prf_task_id1) {
            switch(r_req->att_idx)
            {
                case GATTS_IDX_CHAR_DECL:
                    break;
                case GATTS_IDX_CHAR_VALUE:
                    break;
				case GATTS_IDX_CHAR_DESC:
					// bk_ble_read_response_value(r_req->conn_idx, sizeof(notify_v), &notify_v[0], r_req->prf_id, r_req->att_idx);
					break;
                default:
                    break;
            }
        }
        break;
    }
    case BLE_5_REPORT_ADV: {
        break;
    }
    case BLE_5_MTU_CHANGE: {
        ble_mtu_change_t *m_ind = (ble_mtu_change_t *)param;
		g_mtu = m_ind->mtu_size;
        IOT_INFO("%s m_ind:conn_idx:%d, mtu_size:%d\r\n, gtu_size : %d", __func__, m_ind->conn_idx, m_ind->mtu_size,g_mtu);
        break;
    }
    case BLE_5_CONNECT_EVENT: {
		ble_conn_ind_t *c_ind = (ble_conn_ind_t *)param;
		IOT_INFO("c_ind:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
		c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
		c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);
		gatt_conn_ind = c_ind->conn_idx;
		
		ble_conn_param_t conn_param = {0};
		conn_param.intv_min = 0x10;
		conn_param.intv_max = 0x20;
		conn_param.con_latency = 0;
		conn_param.sup_to = 400;
		conn_param.init_phys = 1;
		bk_ble_update_param(c_ind->conn_idx, &conn_param);
		
		ble_err_t ret = bk_ble_stop_advertising(actv_idx,  ble_gatt_cmd_cb);
		if(ret != BK_ERR_BLE_SUCCESS){
			IOT_INFO("stop ble advertisement failed, error code = 0x%x\n", ret);
		}
		
		if (ble_event_cb) {
			ble_event_cb(IOT_BLE_EVENT_GATT_JOIN, IOT_ERROR_NONE);
		}
		
		gatt_connected = 1;
		indication_need_confirmed = 0;
        break;
    }
    case BLE_5_DISCONNECT_EVENT: {
        ble_discon_ind_t *d_ind = (ble_discon_ind_t *)param;
        IOT_INFO("d_ind:conn_idx:%d,reason:%d\r\n", d_ind->conn_idx, d_ind->reason);
        gatt_conn_ind = ~0;

		bool onboarding_completed = iot_bsp_ble_get_onboarding_completion();
		if (onboarding_completed == false) {
			ble_err_t ret = bk_ble_start_advertising(actv_idx, 0, ble_gatt_cmd_cb);
			if (ret != BK_ERR_BLE_SUCCESS) {
				IOT_INFO("start ble advertisement failed, error code = 0x%x\n", ret);
			}
		}
		if (ble_event_cb) {
			ble_event_cb(IOT_BLE_EVENT_GATT_LEAVE, IOT_ERROR_NONE);
		}
		gatt_connected = 0;
		indication_need_confirmed = 0;
        break;
    }
    case BLE_5_ATT_INFO_REQ: {
        ble_att_info_req_t *a_ind = (ble_att_info_req_t *)param;
        IOT_INFO("a_ind:conn_idx:%d\r\n", a_ind->conn_idx);
        a_ind->length = 128;
        a_ind->status = BK_ERR_BLE_SUCCESS;
        break;
    }
    case BLE_5_CREATE_DB: {
            ble_create_db_t *cd_ind = (ble_create_db_t *)param;

            IOT_INFO("cd_ind:prf_id:%d, status:%d\r\n", cd_ind->prf_id, cd_ind->status);
            gatt_cmd_status = cd_ind->status;
			if (gatt_sema != NULL)
                rtos_set_semaphore( &gatt_sema );
        break;
    }
    case BLE_5_INIT_CONNECT_EVENT: {
        ble_conn_ind_t *c_ind = (ble_conn_ind_t *)param;
        IOT_INFO("BLE_5_INIT_CONNECT_EVENT:conn_idx:%d, addr_type:%d, peer_addr:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                c_ind->conn_idx, c_ind->peer_addr_type, c_ind->peer_addr[0], c_ind->peer_addr[1],
                c_ind->peer_addr[2], c_ind->peer_addr[3], c_ind->peer_addr[4], c_ind->peer_addr[5]);
        break;
    }
    case BLE_5_INIT_DISCONNECT_EVENT: {
        ble_discon_ind_t *d_ind = (ble_discon_ind_t *)param;
        IOT_INFO("BLE_5_INIT_DISCONNECT_EVENT:conn_idx:%d,reason:%d\r\n", d_ind->conn_idx, d_ind->reason);
        break;
    }
    case BLE_5_SDP_REGISTER_FAILED:
        IOT_INFO("BLE_5_SDP_REGISTER_FAILED\r\n");
        break;
    case BLE_5_READ_PHY_EVENT: {
        ble_read_phy_t *phy_param = (ble_read_phy_t *)param;
        IOT_INFO("BLE_5_READ_PHY_EVENT:tx_phy:0x%02x, rx_phy:0x%02x\r\n",phy_param->tx_phy, phy_param->rx_phy);
        break;
    }
    case BLE_5_TX_DONE:
        break;
    case BLE_5_CONN_UPDATA_EVENT: {
        ble_conn_param_t *updata_param = (ble_conn_param_t *)param;
        IOT_INFO("BLE_5_CONN_UPDATA_EVENT:conn_interval:0x%04x, con_latency:0x%04x, sup_to:0x%04x\r\n", updata_param->intv_max,
        updata_param->con_latency, updata_param->sup_to);
        break;
    }
    case BLE_5_PAIRING_REQ:
        printf("BLE_5_PAIRING_REQ\r\n");
        ble_smp_ind_t *s_ind = (ble_smp_ind_t *)param;
        bk_ble_sec_send_auth_mode(s_ind->conn_idx, GAP_AUTH_REQ_NO_MITM_BOND, BK_BLE_GAP_IO_CAP_NO_INPUT_NO_OUTPUT,
            GAP_SEC1_NOAUTH_PAIR_ENC, GAP_OOB_AUTH_DATA_NOT_PRESENT);
        break;

    case BLE_5_PARING_PASSKEY_REQ:
        IOT_INFO("BLE_5_PARING_PASSKEY_REQ\r\n");
        break;

    case BLE_5_ENCRYPT_EVENT:
        IOT_INFO("BLE_5_ENCRYPT_EVENT\r\n");
        break;

    case BLE_5_PAIRING_SUCCEED:
        IOT_INFO("BLE_5_PAIRING_SUCCEED\r\n");
        break;

    case BLE_5_KEY_EVENT:
    {
        IOT_INFO("BLE_5_KEY_EVENT\r\n");
        s_ble_enc_key = *((bk_ble_key_t*)param);
        break;
    }

    case BLE_5_BOND_INFO_REQ_EVENT:
    {
        IOT_INFO("BLE_5_BOND_INFO_REQ_EVENT\r\n");
        // bk_ble_bond_info_req_t *bond_info_req = (bk_ble_bond_info_req_t*)param;
        // if ((bond_info_req->key.peer_addr_type == s_ble_enc_key.peer_addr_type)
        //     && (!os_memcmp(bond_info_req->key.peer_addr, s_ble_enc_key.peer_addr, 6)))
        // {
        //     bond_info_req->key_found = 1;
        //     bond_info_req->key = s_ble_enc_key;
        // }
        break;
    }

    case BLE_5_GAP_CMD_CMP_EVENT:
    {
        ble_cmd_cmp_evt_t *event = (ble_cmd_cmp_evt_t *)param;

        switch(event->cmd) {
        case BLE_CONN_ENCRYPT:
            {
                IOT_INFO("BLE_5_GAP_CMD_CMP_EVENT(BLE_CONN_ENCRYPT) , status %x\r\n",event->status);
                // if (event->status)
                // {
                //     os_memset(&s_ble_enc_key, 0 ,sizeof(s_ble_enc_key));
                //     bk_ble_disconnect(event->conn_idx);
                // }
            }
            break;

        default:
            break;
        }

        break;
    }

    case BLE_5_PAIRING_FAILED:
    {
        IOT_INFO("BLE_5_PAIRING_FAILED\r\n");
        // os_memset(&s_ble_enc_key, 0 ,sizeof(s_ble_enc_key));
        break;
    }
    default:
        break;
    }
}

// #define ADV_MAX_SIZE (251)
// #define ADV_NAME_HEAD "BK"
// #define ADV_TYPE_FLAGS                      (0x01)
// #define ADV_TYPE_LOCAL_NAME                 (0x09)
// #define ADV_TYPE_SERVICE_UUIDS_16BIT        (0x14)
// #define ADV_TYPE_SERVICE_DATA               (0x16)
// #define ADV_TYPE_MANUFACTURER_SPECIFIC      (0xFF)
// #define BEKEN_COMPANY_ID                    (0x05F0)
#define GATTS_UUID                       (0xFD1D)


static int32_t bk_gatts_at_cb(bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *comm_param)
{
	// struct gatts_reg_evt_param *param = (typeof(param))comm_param;
    switch (event)
    {
		case BK_GATTS_CONF_EVT:
		{
			IOT_INFO(" BK_GATTS_CONF_EVT");
			// if (param->conf.status != ESP_GATT_OK){
		// 	esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
		// }
			indication_need_confirmed = 0;
			break;
		}
		case BK_GATTS_REG_EVT:
		case BK_GATTS_READ_EVT:
		case BK_GATTS_WRITE_EVT:
		case BK_GATTS_EXEC_WRITE_EVT:


		case BK_GATTS_MTU_EVT:
		case BK_GATTS_CREATE_EVT:
		case BK_GATTS_ADD_CHAR_EVT:
		case BK_GATTS_ADD_CHAR_DESCR_EVT:

		case BK_GATTS_START_EVT:
		case BK_GATTS_STOP_EVT:
		case BK_GATTS_CONNECT_EVT:
		case BK_GATTS_DISCONNECT_EVT:
		case BK_GATTS_RESPONSE_EVT:
		case BK_GATTS_CREAT_ATTR_TAB_EVT:
		case BK_GATTS_SET_ATTR_VAL_EVT:
		case BK_GATTS_UNREG_EVT:
		case BK_GATTS_SEND_SERVICE_CHANGE_EVT:





		break;
	}
	return 0;
}
void iot_bsp_ble_init(CharWriteCallback cb){
	bt_err_t ret = BK_FAIL;
    struct bk_ble_db_cfg ble_db_cfg;

    ret = rtos_init_semaphore(&gatt_sema, 2);
    if(ret != BK_OK){
        IOT_INFO("gatts init sema fial \n");
        return;
    }
	CharWriteCb = cb;
    bk_ble_set_notice_cb(ble_gatts_notice_cb);

	  bk_ble_gatts_register_callback(bk_gatts_at_cb);

    ble_db_cfg.att_db = (ble_attm_desc_t *)gatts_service_db;
    ble_db_cfg.att_db_nb = GATTS_IDX_NB;
    ble_db_cfg.prf_task_id = PRF_TASK_ID_GATTS;
    ble_db_cfg.start_hdl = 0;
    ble_db_cfg.svc_perm = BK_BLE_PERM_SET(SVC_UUID_LEN, UUID_16);
    ble_db_cfg.uuid[0] = GATTS_UUID & 0xFF;
    ble_db_cfg.uuid[1] = GATTS_UUID >> 8;

    ret = bk_ble_create_db(&ble_db_cfg);

    if (ret != BK_ERR_BLE_SUCCESS)
    {
        IOT_INFO("create gatt db failed %d\n", ret);
        return;
		
    }

    ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

    if (ret != BK_OK)
    {
        IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
        //goto error;
    }
    else
    {
        IOT_INFO("create gatt db success\n");
    }

    //set adv data
    // uint8_t adv_data1[ADV_MAX_SIZE] = {0};
	// uint8_t adv_index = 0;
	// uint8_t len_index = 0;
	// uint8_t mac[6];

	// /* flags */
	// len_index = adv_index;
	// adv_data1[adv_index++] = 0x00;
	// adv_data1[adv_index++] = ADV_TYPE_FLAGS;
	// adv_data1[adv_index++] = 0x06;
	// adv_data1[len_index] = 2;

	// /* local name */
	// bk_bluetooth_get_address(mac);

	// len_index = adv_index;
	// adv_data1[adv_index++] = 0x00;
	// adv_data1[adv_index++] = ADV_TYPE_LOCAL_NAME;

	// ret = sprintf((char *)&adv_data1[adv_index], "%s_%02X%02X%02X",
	//               ADV_NAME_HEAD, mac[0], mac[1], mac[2]);

    // bk_ble_appm_set_dev_name(ret, &adv_data1[adv_index]);
    // IOT_INFO("%s, dev_name:%s, ret:%d \n", __func__, (char *)&adv_data1[adv_index], ret);
	// adv_index += ret;
	// adv_data1[len_index] = ret + 1;

	// /* 16bit uuid */
	// len_index = adv_index;
	// adv_data1[adv_index++] = 0x00;
	// adv_data1[adv_index++] = ADV_TYPE_SERVICE_DATA;
	// adv_data1[adv_index++] = GATTS_UUID & 0xFF;
	// adv_data1[adv_index++] = GATTS_UUID >> 8;
	// adv_data1[len_index] = 3;

	// /* manufacturer */
	// len_index = adv_index;
	// adv_data1[adv_index++] = 0x00;
	// adv_data1[adv_index++] = ADV_TYPE_MANUFACTURER_SPECIFIC;
	// adv_data1[adv_index++] = BEKEN_COMPANY_ID & 0xFF;
	// adv_data1[adv_index++] = BEKEN_COMPANY_ID >> 8;
	// adv_data1[len_index] = 3;
    // IOT_INFO("adv data length :%d \n", adv_index);

	/* set adv paramters */
    ble_adv_param_t adv_param;
	int actv_idx = 0;

	memset(&adv_param, 0, sizeof(ble_adv_param_t));
	adv_param.chnl_map = ADV_ALL_CHNLS;
	adv_param.adv_intv_min = BLE_GATTS_ADV_INTERVAL_MIN;
	adv_param.adv_intv_max = BLE_GATTS_ADV_INTERVAL_MAX;

#if 1
	adv_param.own_addr_type = OWN_ADDR_TYPE_PUBLIC_ADDR;
#else
	adv_param.own_addr_type = OWN_ADDR_TYPE_RANDOM_ADDR;
#endif

	adv_param.adv_type = ADV_TYPE_LEGACY;
	adv_param.adv_prop = ADV_PROP_CONNECTABLE_BIT | ADV_PROP_SCANNABLE_BIT;
	adv_param.prim_phy = INIT_PHY_TYPE_LE_1M;
	adv_param.second_phy = INIT_PHY_TYPE_LE_1M;

	ret = bk_ble_create_advertising(actv_idx, &adv_param, ble_gatt_cmd_cb);

	if (ret != BK_ERR_BLE_SUCCESS)
	{
		IOT_INFO("config adv paramters failed %d\n", ret);
		// goto error;
	}

	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

	if (ret != BK_OK)
	{
		IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
		// goto error;
	}
	else
	{
		IOT_INFO("set adv paramters success\n");
	}

	/* set adv paramters */
	ret = bk_ble_set_adv_data(actv_idx, adv_data, 31, ble_gatt_cmd_cb);

	if (ret != BK_ERR_BLE_SUCCESS)
	{
		IOT_INFO("set adv data failed %d\n", ret);
		// goto error;
	}

	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

	if (ret != BK_OK)
	{
		IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
		// goto error;
	}
	else
	{
		IOT_INFO("set adv data success\n");

		
	}

		//set scan response
	bk_ble_set_scan_rsp_data(actv_idx, scan_response_data, scan_response_len, ble_gatt_cmd_cb);

	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

	if (ret != BK_OK)
	{
		IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
		// goto error;
	}
	else
	{
		IOT_INFO("set adv data success\n");

		
	}
	ret = bk_ble_start_advertising(actv_idx, 0, ble_gatt_cmd_cb);

	if (ret != BK_ERR_BLE_SUCCESS)
	{
		IOT_INFO("start adv failed %d\n", ret);
		//goto error;
	}

	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

	if (ret != BK_OK)
	{
		IOT_INFO("wait semaphore failed at %d, %d\n", ret, __LINE__);
		//goto error;
	}
	else
	{
		IOT_INFO("start adv success\n");
	}

	g_mtu = GATTS_MTU_MAX;
	bt_err_t local_mtu_ret = bk_ble_set_max_mtu(g_mtu);
	if (local_mtu_ret){
		IOT_INFO(GATTS_TAG,"set local  MTU failed, error code = %x\n", local_mtu_ret);
	}
	IOT_INFO("ble init done\n");
	return;

}

// void iot_bsp_ble_init(CharWriteCallback cb)
// {
// 	IOT_INFO("");
// // #if (CONFIG_BLUETOOTH)
// // 	app_bt_init();
// // 	app_ble_init();
// // #endif
// 	printf("dayinwodexinxi\r\n");
// 	char tStr[40] = "This is a test log";
// 	printf("%s\r\n", tStr);		
// 	bt_err_t ret;
// 	ret = bk_bluetooth_init();
// 	if(ret) {
// 		IOT_INFO("failed to init bk ble\n");
// 		return;
// 	}

// 	struct bk_ble_db_cfg ble_db_cfg;

// ret = rtos_init_semaphore(&gatt_sema, 1);
//     if(ret != BK_OK){
//         IOT_ERROR("gatts init sema fial \n");
//         return;
//     }


// 	if (g_ble_status == Bk_BLE_STATUS_INIT) {
// 		IOT_INFO("BK BLE already initialised");
// 		return;
// 	}

// 	CharWriteCb = cb;



// 	g_ble_status = Bk_BLE_STATUS_INIT;

// 	bk_ble_set_notice_cb(ble_gatts_notice_cb);	
	
// 	//if (g_onboarding_complete == false) {
// 		ble_db_cfg.att_db = (ble_attm_desc_t *)gatts_service_db;
// 		ble_db_cfg.att_db_nb = GATTS_IDX_NB;
// 		ble_db_cfg.prf_task_id = PRF_TASK_ID_GATTS;
// 		ble_db_cfg.start_hdl = 0;
// 		ble_db_cfg.svc_perm = BK_BLE_PERM_SET(SVC_UUID_LEN, UUID_16);
// 		ble_db_cfg.uuid[0] = GATTS_SERVICE_UUID & 0xFF;
// 		ble_db_cfg.uuid[1] = GATTS_SERVICE_UUID >> 8;

// 		ret = bk_ble_create_db(&ble_db_cfg);
// 		if (ret != BK_ERR_BLE_SUCCESS){
// 			IOT_ERROR(GATTS_TAG,"create gatt db failed %d\n", ret);
// 			return;
// 		}
// 	//}

// 	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);
//   if (ret != BK_OK)
//     {
//         IOT_ERROR("wait semaphore failed at %d, %d\n", ret, __LINE__);
//         // goto error;
//     }
//     else
//     {
//         IOT_INFO("create gatt db success\n");
//     }

// 	memset(&adv_param, 0, sizeof(ble_adv_param_t));
// 	adv_param.chnl_map = ADV_ALL_CHNLS;
// 	adv_param.adv_intv_min = BLE_GATTS_ADV_INTERVAL_MIN;
// 	adv_param.adv_intv_max = BLE_GATTS_ADV_INTERVAL_MAX;
// 	adv_param.own_addr_type = OWN_ADDR_TYPE_PUBLIC_ADDR;
// 	adv_param.adv_type = ADV_TYPE_LEGACY;
// 	adv_param.adv_prop = ADV_PROP_CONNECTABLE_BIT | ADV_PROP_SCANNABLE_BIT;
// 	adv_param.prim_phy = INIT_PHY_TYPE_LE_1M;
// 	adv_param.second_phy = INIT_PHY_TYPE_LE_1M;
// 	// actv_idx = bk_ble_get_idle_actv_idx_handle();
// 	actv_idx = 0;
// 	IOT_INFO("actv_idx = %d",actv_idx);
// 	//if (actv_idx != UNKNOW_ACT_IDX) {
// 	ret = bk_ble_create_advertising(actv_idx, &adv_param, ble_gatt_cmd_cb);
// 	if (ret != BK_ERR_BLE_SUCCESS)
// 	{
// 		IOT_ERROR("config adv paramters failed %d\n", ret);
// 		return;
// 	}

// 	//}

// 	ret = rtos_get_semaphore(&gatt_sema, GATT_SYNC_CMD_TIMEOUT_MS);

// 	if (ret != BK_OK)
// 	{
// 		IOT_ERROR("wait semaphore failed at %d, %d\n", ret, __LINE__);
// 		//return;
// 	}
// 	else
// 	{
// 		IOT_INFO("set adv paramters success\n");
// 	}
// 	/* set adv paramters */
// 	uint8_t adv_data[] = {0x02, 0x01, 0x06, 0x0A, 0x09, 0x37 ,0x32, 0x33, 0x31, 0x4e, 0x5f, 0x42, 0x4c, 0x45};
// 	ret = bk_ble_set_adv_data(actv_idx, adv_data, sizeof(adv_data), ble_gatt_cmd_cb);
// 	if(ret != BK_ERR_BLE_SUCCESS)
// 	{
// 		IOT_ERROR("set adv data failed %d\n", ret);
// 		return;
// 	}

// 	//set scan response
// 	bk_ble_set_scan_rsp_data(actv_idx, scan_response_data, scan_response_len, ble_gatt_cmd_cb);

// 	/* sart adv */
// 	ret = bk_ble_start_advertising(actv_idx, 0, ble_gatt_cmd_cb);
// 	if (ret != BK_ERR_BLE_SUCCESS)
// 	{
// 		IOT_INFO("start adv failed %d\n", ret);
// 		return;
// 	}

// 	g_mtu = GATTS_MTU_MAX;
// 	bt_err_t local_mtu_ret = bk_ble_set_max_mtu(g_mtu);
// 	if (local_mtu_ret){
// 		IOT_INFO(GATTS_TAG,"set local  MTU failed, error code = %x\n", local_mtu_ret);
// 	}
// 	IOT_INFO("ble init done\n");
// 	return;
// }

iot_error_t iot_bsp_ble_register_event_cb(iot_bsp_ble_event_cb_t cb)
{
	if (cb == NULL)
	{
		return IOT_ERROR_INVALID_ARGS;
	}

	ble_event_cb = cb;
	return IOT_ERROR_NONE;
}


void iot_bsp_ble_deinit(void)
{
	bt_err_t ret;

	if (g_ble_status != Bk_BLE_STATUS_INIT) {
		IOT_INFO(GATTS_TAG, "BK BLE already deinitalised");
		return;
	}

	ret = bk_bluetooth_deinit();
	if (ret) {
		IOT_INFO(GATTS_TAG, "%s disable bluetooth failed\n", __func__);
		return;
	}

	g_ble_status = Bk_BLE_STATUS_DEINIT;
	CharWriteCb = NULL;
	ble_event_cb = NULL;
	return;
}

uint32_t iot_bsp_ble_get_mtu(void)
{
	return g_mtu;
}
