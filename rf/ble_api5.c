
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf_sdm.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_db_discovery.h"
#include "ble_srv_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
#include "nrf_pwr_mgmt.h"
#include "app_util.h"
#include "app_error.h"
#include "peer_manager.h"
#include "app_util.h"
#include "app_timer.h"
#include "bsp_btn_ble.h"
#include "fds.h"
#include "nrf_fstorage.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "helper.h"
#include "ble_bas_c.h"
#include "ble_nus_c.h"
#include "ble_api_base.h"
#include "ble_komoot_c.h"
#include "ble_advdata.h"
#include "ble_lns_c.h"
#include "ant.h"
#include "glasses.h"
#include "Model.h"
#include "Locator.h"
#include "neopixel.h"
#include "segger_wrapper.h"
#include "ring_buffer.h"
#include "Model.h"

#define BLE_DEVICE_NAME             "myStrava"

#define APP_BLE_CONN_CFG_TAG        1                                   /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_BLE_OBSERVER_PRIO       1                                   /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_SOC_OBSERVER_PRIO       1                                   /**< Applications' SoC observer priority. You shoulnd't need to modify this value. */


#define SEC_PARAM_BOND              1                                   /**< Perform bonding. */
#define SEC_PARAM_MITM              0                                   /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC              0                                   /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS          0                                   /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES   BLE_GAP_IO_CAPS_NONE                /**< No I/O capabilities. */
#define SEC_PARAM_OOB               0                                   /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE      7                                   /**< Minimum encryption key size in octets. */
#define SEC_PARAM_MAX_KEY_SIZE      16                                  /**< Maximum encryption key size in octets. */

#define SCAN_INTERVAL               0x00A0                              /**< Determines scan interval in units of 0.625 millisecond. */
#define SCAN_WINDOW                 0x0050                              /**< Determines scan window in units of 0.625 millisecond. */
#define SCAN_DURATION               0x0000                              /**< Duration of the scanning in units of 10 milliseconds. If set to 0x0000, scanning will continue until it is explicitly disabled. */
#define SCAN_DURATION_WITELIST      3000                                /**< Duration of the scanning in units of 10 milliseconds. */

#define MIN_CONNECTION_INTERVAL     MSEC_TO_UNITS(7.5, UNIT_1_25_MS)    /**< Determines minimum connection interval in millisecond. */
#define MAX_CONNECTION_INTERVAL     MSEC_TO_UNITS(30, UNIT_1_25_MS)     /**< Determines maximum connection interval in millisecond. */
#define SLAVE_LATENCY               0                                   /**< Determines slave latency in counts of connection events. */
#define SUPERVISION_TIMEOUT         MSEC_TO_UNITS(4000, UNIT_10_MS)     /**< Determines supervision time-out in units of 10 millisecond. */

#define TARGET_UUID                 BLE_UUID_LOCATION_AND_NAVIGATION_SERVICE         /**< Target device name that application is looking for. */
#define TARGET_NAME                 "stravaAP"


/**@breif Macro to unpack 16bit unsigned UUID from octet stream. */
#define UUID16_EXTRACT(DST, SRC) \
		do                           \
		{                            \
			(*(DST))   = (SRC)[1];   \
			(*(DST)) <<= 8;          \
			(*(DST))  |= (SRC)[0];   \
		} while (0)


/**@brief Variable length data encapsulation in terms of length and pointer to data */
typedef struct
{
	uint8_t  * p_data;      /**< Pointer to data. */
	uint16_t   data_len;    /**< Length of data. */
} data_t;

typedef enum {
	eNusTransferStateIdle,
	eNusTransferStateInit,
	eNusTransferStateRun,
	eNusTransferStateWait,
	eNusTransferStateFinish
} eNusTransferState;

BLE_NUS_C_DEF(m_ble_nus_c);
BLE_KOMOOT_C_DEF(m_ble_komoot_c);
BLE_LNS_C_DEF(m_ble_lns_c);                                             /**< Structure used to identify the heart rate client module. */
NRF_BLE_GATT_DEF(m_gatt);                                           /**< GATT module instance. */
BLE_DB_DISCOVERY_DEF(m_db_disc);                                    /**< DB discovery module instance. */

#define NUS_RB_SIZE      1024
RING_BUFFER_DEF(nus_rb1, NUS_RB_SIZE);

/** @brief Parameters used when scanning. */
static ble_gap_scan_params_t m_scan_param;

//static uint16_t              m_conn_handle;                /**< Current connection handle. */
static bool                  m_whitelist_disabled;         /**< True if whitelist has been temporarily disabled. */
static bool                  m_memory_access_in_progress;  /**< Flag to keep track of ongoing operations on persistent memory. */

static bool                  m_retry_db_disc;              /**< Flag to keep track of whether the DB discovery should be retried. */
static uint16_t              m_pending_db_disc_conn = BLE_CONN_HANDLE_INVALID;  /**< Connection handle for which the DB discovery is retried. */

static volatile bool m_nus_cts = false;
static volatile bool m_connected = false;
static uint16_t m_nus_packet_nb = 0;

static eNusTransferState m_nus_xfer_state = eNusTransferStateIdle;

/**@brief Connection parameters requested for connection. */
static ble_gap_conn_params_t const m_connection_param =
{
		(uint16_t)MIN_CONNECTION_INTERVAL,  /**< Minimum connection. */
		(uint16_t)MAX_CONNECTION_INTERVAL,  /**< Maximum connection. */
		(uint16_t)SLAVE_LATENCY,            /**< Slave latency. */
		(uint16_t)SUPERVISION_TIMEOUT       /**< Supervision time-out. */
};


static void scan_start(void);




/**@brief Function for handling database discovery events.
 *
 * @details This function is callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function should forward the events
 *          to their respective services.
 *
 * @param[in] p_event  Pointer to the database discovery event.
 */
static void db_disc_handler(ble_db_discovery_evt_t * p_evt)
{
	ble_lns_c_on_db_disc_evt(&m_ble_lns_c, p_evt);
	ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
	ble_komoot_c_on_db_disc_evt(&m_ble_komoot_c, p_evt);
}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
	ret_code_t err_code;

	switch (p_evt->evt_id)
	{
	case PM_EVT_BONDED_PEER_CONNECTED:
	{
		LOG_INFO("Connected to a previously bonded device.");
	} break;

	case PM_EVT_CONN_SEC_SUCCEEDED:
	{
		LOG_INFO("Connection secured: role: %d, conn_handle: 0x%x, procedure: %d.",
				ble_conn_state_role(p_evt->conn_handle),
				p_evt->conn_handle,
				p_evt->params.conn_sec_succeeded.procedure);
	} break;

	case PM_EVT_CONN_SEC_FAILED:
	{
		/* Often, when securing fails, it shouldn't be restarted, for security reasons.
		 * Other times, it can be restarted directly.
		 * Sometimes it can be restarted, but only after changing some Security Parameters.
		 * Sometimes, it cannot be restarted until the link is disconnected and reconnected.
		 * Sometimes it is impossible, to secure the link, or the peer device does not support it.
		 * How to handle this error is highly application dependent. */
	} break;

	case PM_EVT_CONN_SEC_CONFIG_REQ:
	{
		// Reject pairing request from an already bonded peer.
		pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
		pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
	} break;

	case PM_EVT_STORAGE_FULL:
	{
		// Run garbage collection on the flash.
		err_code = fds_gc();
		if (err_code == FDS_ERR_BUSY || err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
		{
			// Retry.
		}
		else
		{
			APP_ERROR_CHECK(err_code);
		}
	} break;

	case PM_EVT_PEERS_DELETE_SUCCEEDED:
	{
		// Bonds are deleted. Start scanning.
		scan_start();
	} break;

	case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
	{
		// The local database has likely changed, send service changed indications.
		pm_local_database_has_changed();
	} break;

	case PM_EVT_PEER_DATA_UPDATE_FAILED:
	{
		// Assert.
		APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
	} break;

	case PM_EVT_PEER_DELETE_FAILED:
	{
		// Assert.
		APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
	} break;

	case PM_EVT_PEERS_DELETE_FAILED:
	{
		// Assert.
		APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
	} break;

	case PM_EVT_ERROR_UNEXPECTED:
	{
		// Assert.
		APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
	} break;

	case PM_EVT_CONN_SEC_START:
	case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
	case PM_EVT_PEER_DELETE_SUCCEEDED:
	case PM_EVT_LOCAL_DB_CACHE_APPLIED:
	case PM_EVT_SERVICE_CHANGED_IND_SENT:
	case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
	default:
		break;
	}
}

/**@brief Function for handling the advertising report BLE event.
 *
 * @param[in] p_adv_report  Advertising report from the SoftDevice.
 */
static void on_adv_report(ble_gap_evt_adv_report_t const * p_adv_report)
{
	ret_code_t err_code;
	ble_uuid_t target_uuid1 = {.uuid = TARGET_UUID, .type = BLE_UUID_TYPE_BLE};
	ble_uuid_t target_uuid2 = {.uuid = BLE_UUID_KOMOOT_SERVICE, .type = m_ble_komoot_c.uuid_type};

	bool do_connect = false;

	if (ble_advdata_uuid_find(p_adv_report->data, p_adv_report->dlen, &target_uuid1))
	{
		do_connect = true;
		LOG_INFO("UUID1 match send connect_request.");
	}
	if (ble_advdata_uuid_find(p_adv_report->data, p_adv_report->dlen, &target_uuid2))
	{
		do_connect = true;
		LOG_INFO("UUID2 match send connect_request.");
	}
	if (ble_advdata_name_find(p_adv_report->data, p_adv_report->dlen, TARGET_NAME)) {
		do_connect = true;
		LOG_INFO("Name match send connect_request.");
	}

    if (do_connect)
    {
        // Stop scanning.
        (void) sd_ble_gap_scan_stop();

        // Initiate connection.
        err_code = sd_ble_gap_connect(&p_adv_report->peer_addr,
                                      &m_scan_param,
                                      &m_connection_param,
                                      APP_BLE_CONN_CFG_TAG);

        m_whitelist_disabled = false;

        if (err_code != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("Connection Request Failed, reason %d.", err_code);
        }
    }
    else
    {
    	sd_ble_gap_scan_stop();
        err_code = sd_ble_gap_scan_start(&m_scan_param);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
	ret_code_t            err_code;
	ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

	W_SYSVIEW_RecordEnterISR();

	switch (p_ble_evt->header.evt_id)
	{
	case BLE_GAP_EVT_CONNECTED:
	{
		LOG_INFO("Connected.");
		m_connected = true;
		m_pending_db_disc_conn = p_ble_evt->evt.gap_evt.conn_handle;
		m_retry_db_disc = false;
		// Discover peer's services.
		err_code = ble_db_discovery_start(&m_db_disc, m_pending_db_disc_conn);
		if (err_code == NRF_ERROR_BUSY)
		{
			LOG_INFO("ble_db_discovery_start() returned busy, will retry later.");
			m_retry_db_disc = true;
		}
		else
		{
			APP_ERROR_CHECK(err_code);
		}

		// TODO
//		if (ble_conn_state_n_centrals() < NRF_SDH_BLE_CENTRAL_LINK_COUNT)
//		{
//			scan_start();
//		}
	} break;

	case BLE_GAP_EVT_ADV_REPORT:
	{
		on_adv_report(&p_gap_evt->params.adv_report);
	} break; // BLE_GAP_EVT_ADV_REPORT

	case BLE_GAP_EVT_DISCONNECTED:
	{
		LOG_INFO("Disconnected, reason 0x%x.",
				p_ble_evt->evt.gap_evt.params.disconnected.reason);

		m_connected = false;

		// Reset DB discovery structure.
		memset(&m_db_disc, 0 , sizeof (m_db_disc));

		// TODO
//		if (ble_conn_state_n_centrals() < NRF_SDH_BLE_CENTRAL_LINK_COUNT)
		{
			scan_start();
		}
	} break;

	case BLE_GAP_EVT_TIMEOUT:
	{
		if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN)
		{
			NRF_LOG_DEBUG("Scan timed out.");
			scan_start();
		}
		else if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
		{
			LOG_INFO("Connection Request timed out.");
		}
	} break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		// Accepting parameters requested by peer.
		err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle,
				&p_gap_evt->params.conn_param_update_request.conn_params);
		APP_ERROR_CHECK(err_code);
		break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
	{
		NRF_LOG_DEBUG("PHY update request.");
		ble_gap_phys_t const phys =
		{
				.rx_phys = BLE_GAP_PHY_AUTO,
				.tx_phys = BLE_GAP_PHY_AUTO,
		};
		err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
		APP_ERROR_CHECK(err_code);
	} break;

	case BLE_GATTC_EVT_TIMEOUT:
		// Disconnect on GATT Client timeout event.
		NRF_LOG_DEBUG("GATT Client Timeout.");
		err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
				BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		APP_ERROR_CHECK(err_code);
		break;

	case BLE_GATTS_EVT_TIMEOUT:
		// Disconnect on GATT Server timeout event.
		NRF_LOG_DEBUG("GATT Server Timeout.");
		err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
				BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		APP_ERROR_CHECK(err_code);
		break;

	case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
		NRF_LOG_INFO("GATTC HVX Complete");
		// clear to send more packets
		m_nus_cts = true;
		break;

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		// unused here
		break;

	default:
		break;
	}

	W_SYSVIEW_RecordExitISR();

}


/**@brief SoftDevice SoC event handler.
 *
 * @param[in]   evt_id      SoC event.
 * @param[in]   p_context   Context.
 */
static void soc_evt_handler(uint32_t evt_id, void * p_context)
{
	switch (evt_id)
	{
	case NRF_EVT_FLASH_OPERATION_SUCCESS:
		/* fall through */
	case NRF_EVT_FLASH_OPERATION_ERROR:

		if (m_memory_access_in_progress)
		{
			m_memory_access_in_progress = false;
			scan_start();
		}
		break;

	default:
		// No implementation needed.
		break;
	}
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
	ret_code_t err_code;

	// Configure the BLE stack using the default settings.
	// Fetch the start address of the application RAM.
	uint32_t ram_start = 0;
	err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
	APP_ERROR_CHECK(err_code);

	NRF_LOG_WARNING("Ram start1: %x", ram_start);
	NRF_LOG_FLUSH();

	// Enable BLE stack.
	err_code = nrf_sdh_ble_enable(&ram_start);
	APP_ERROR_CHECK(err_code);

	NRF_LOG_WARNING("Ram start2: %x", ram_start);
	NRF_LOG_FLUSH();

	// Register handlers for BLE and SoC events.
	NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
	NRF_SDH_SOC_OBSERVER(m_soc_observer, APP_SOC_OBSERVER_PRIO, soc_evt_handler, NULL);

	// set name
	ble_gap_conn_sec_mode_t sec_mode; // Struct to store security parameters
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
	/*Get this device name*/
	uint8_t device_name[20];
	memset(device_name, 0, sizeof(device_name));
	memcpy(device_name, BLE_DEVICE_NAME, strlen(BLE_DEVICE_NAME));
	err_code = sd_ble_gap_device_name_set(&sec_mode, device_name, strlen(BLE_DEVICE_NAME));
}



/**@brief Function for the Peer Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Peer Manager.
 */
static void peer_manager_init(void)
{
	ble_gap_sec_params_t sec_param;
	ret_code_t err_code;

	err_code = pm_init();
	APP_ERROR_CHECK(err_code);

	memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

	// Security parameters to be used for all security procedures.
	sec_param.bond           = SEC_PARAM_BOND;
	sec_param.mitm           = SEC_PARAM_MITM;
	sec_param.lesc           = SEC_PARAM_LESC;
	sec_param.keypress       = SEC_PARAM_KEYPRESS;
	sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
	sec_param.oob            = SEC_PARAM_OOB;
	sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
	sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
	sec_param.kdist_own.enc  = 1;
	sec_param.kdist_own.id   = 1;
	sec_param.kdist_peer.enc = 1;
	sec_param.kdist_peer.id  = 1;

	err_code = pm_sec_params_set(&sec_param);
	APP_ERROR_CHECK(err_code);

	err_code = pm_register(pm_evt_handler);
	APP_ERROR_CHECK(err_code);
}

/**@brief Heart Rate Collector Handler.
 */
static void lns_c_evt_handler(ble_lns_c_t * p_lns_c, ble_lns_c_evt_t * p_lns_c_evt)
{
	uint32_t err_code;

	NRF_LOG_DEBUG("LNS event: 0x%X\r\n", p_lns_c_evt->evt_type);

	switch (p_lns_c_evt->evt_type)
	{
	case BLE_LNS_C_EVT_DISCOVERY_COMPLETE:
		err_code = ble_lns_c_handles_assign(p_lns_c ,
				p_lns_c_evt->conn_handle,
				&p_lns_c_evt->params.peer_db);
		APP_ERROR_CHECK(err_code);

		// Initiate bonding.
		err_code = pm_conn_secure(p_lns_c_evt->conn_handle, false);
		if (err_code != NRF_ERROR_INVALID_STATE)
		{
			APP_ERROR_CHECK(err_code);
		}

		// LNS service discovered. Enable notification of LNS.
		err_code = ble_lns_c_pos_notif_enable(p_lns_c);
		APP_ERROR_CHECK(err_code);

		NRF_LOG_DEBUG("LNS service discovered.");
		break;

	case BLE_LNS_C_EVT_LNS_NOTIFICATION:
	{
		sLnsInfo lns_info;

		lns_info.lat = p_lns_c_evt->params.lns.lat;
		lns_info.lon = p_lns_c_evt->params.lns.lon;
		lns_info.ele = 0;
		lns_info.speed = 0;

		LOG_INFO("Latitude  = %ld", p_lns_c_evt->params.lns.lat);
		LOG_INFO("Longitude = %ld", p_lns_c_evt->params.lns.lon);

		LOG_INFO("Ele %ld", p_lns_c_evt->params.lns.ele);

		lns_info.secj = p_lns_c_evt->params.lns.utc_time.seconds;
		lns_info.secj += p_lns_c_evt->params.lns.utc_time.minutes * 60;
		lns_info.secj += p_lns_c_evt->params.lns.utc_time.hours * 3600;

		lns_info.date = p_lns_c_evt->params.lns.utc_time.year   % 100;
		lns_info.date += p_lns_c_evt->params.lns.utc_time.day   * 10000;
		lns_info.date += p_lns_c_evt->params.lns.utc_time.month * 100;

		if (p_lns_c_evt->params.lns.flags & ELE_PRESENT) {
			lns_info.ele = p_lns_c_evt->params.lns.ele;
		}

		if (p_lns_c_evt->params.lns.flags & INST_SPEED_PRESENT) {
			lns_info.speed = p_lns_c_evt->params.lns.inst_speed;
		}

		if (p_lns_c_evt->params.lns.flags & HEADING_PRESENT) {
			lns_info.heading = p_lns_c_evt->params.lns.heading;
		} else {
			lns_info.heading = -1;
		}

		locator_dispatch_lns_update(&lns_info);

		LOG_INFO("Sec jour = %d %d %d", p_lns_c_evt->params.lns.utc_time.hours,
				p_lns_c_evt->params.lns.utc_time.minutes,
				p_lns_c_evt->params.lns.utc_time.seconds);

		break;
	}

	default:
		break;
	}
}


/**@brief Heart Rate Collector Handler.
 */
static void komoot_c_evt_handler(ble_komoot_c_t * p_komoot_c, ble_komoot_c_evt_t * p_komoot_c_evt)
{
	uint32_t err_code;

	LOG_DEBUG("KOMOOT event: 0x%X\r\n", p_komoot_c_evt->evt_type);

	switch (p_komoot_c_evt->evt_type)
	{
	case BLE_KOMOOT_C_EVT_DISCOVERY_COMPLETE:
		err_code = ble_komoot_c_handles_assign(p_komoot_c ,
				p_komoot_c_evt->conn_handle,
				&p_komoot_c_evt->params.peer_db);
		APP_ERROR_CHECK(err_code);

		// Initiate bonding.
		err_code = pm_conn_secure(p_komoot_c_evt->conn_handle, false);
		if (err_code != NRF_ERROR_INVALID_STATE)
		{
			APP_ERROR_CHECK(err_code);
		}

		// service discovered. Enable notification
		err_code = ble_komoot_c_pos_notif_enable(p_komoot_c);
		APP_ERROR_CHECK(err_code);

		LOG_INFO("KOMOOT service discovered.");
		break;

	case BLE_KOMOOT_C_EVT_KOMOOT_NOTIFICATION:
	{
		uint32_t err_code = ble_komoot_c_nav_read(p_komoot_c);
		APP_ERROR_CHECK(err_code);
	}	break;

    case BLE_KOMOOT_C_EVT_KOMOOT_NAVIGATION:
    {
    	m_komoot_nav.isUpdated = true;
    	m_komoot_nav.direction = p_komoot_c_evt->params.komoot.direction;
    	m_komoot_nav.distance = p_komoot_c_evt->params.komoot.distance;

    	LOG_INFO("KOMOOT nav: direction %u", p_komoot_c_evt->params.komoot.direction);
    }   break;

	default:
		break;
	}
}


/**@brief Battery level Collector Handler.
 */
static void nus_c_evt_handler(ble_nus_c_t * p_ble_nus_c, ble_nus_c_evt_t const * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_type)
    {
        case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
            LOG_INFO("Discovery complete.");
            err_code = ble_nus_c_handles_assign(p_ble_nus_c, p_evt->conn_handle, &p_evt->handles);
            APP_ERROR_CHECK(err_code);

            err_code = ble_nus_c_tx_notif_enable(p_ble_nus_c);
            APP_ERROR_CHECK(err_code);
            LOG_INFO("Connected to stravaAP");
            break;

        case BLE_NUS_C_EVT_NUS_TX_EVT:
            // TODO handle received chars
        	LOG_INFO("Received %u chars from BLE !", p_evt->data_len);
//        	m_nus_xfer_state = eNusTransferStateInit;
        	// ble_nus_chars_received_uart_print(p_ble_nus_evt->p_data, p_ble_nus_evt->data_len);

    		{
    			for (uint16_t i=0; i < p_evt->data_len; i++) {

    				char c = p_evt->p_data[i];

    				if (RING_BUFF_IS_NOT_FULL(nus_rb1)) {
    					RING_BUFFER_ADD_ATOMIC(nus_rb1, c);
    				} else {
    					LOG_ERROR("NUS ring buffer full");

    					// empty ring buffer
    					RING_BUFF_EMPTY(nus_rb1);
    				}

    			}
    		}
            break;

        case BLE_NUS_C_EVT_DISCONNECTED:
    		if (m_nus_xfer_state == eNusTransferStateRun) m_nus_xfer_state = eNusTransferStateFinish;
            break;
    }
}


/**
 * @brief Heart rate collector initialization.
 */
static void lns_c_init(void)
{
	ble_lns_c_init_t lns_c_init_obj;

	lns_c_init_obj.evt_handler = lns_c_evt_handler;

	uint32_t err_code = ble_lns_c_init(&m_ble_lns_c, &lns_c_init_obj);
	APP_ERROR_CHECK(err_code);
}


/**
 * @brief Heart rate collector initialization.
 */
static void komoot_c_init(void)
{
	ble_komoot_c_init_t komoot_c_init_obj;

	komoot_c_init_obj.evt_handler = komoot_c_evt_handler;

	uint32_t err_code = ble_komoot_c_init(&m_ble_komoot_c, &komoot_c_init_obj);
	APP_ERROR_CHECK(err_code);
}


/**
 * @brief Battery level collector initialization.
 */
static void nus_c_init(void)
{
	ble_nus_c_init_t nus_c_init_obj;

	nus_c_init_obj.evt_handler = nus_c_evt_handler;

	uint32_t err_code = ble_nus_c_init(&m_ble_nus_c, &nus_c_init_obj);
	APP_ERROR_CHECK(err_code);
}


/**
 * @brief Database discovery collector initialization.
 */
static void db_discovery_init(void)
{
	ret_code_t err_code = ble_db_discovery_init(db_disc_handler);
	APP_ERROR_CHECK(err_code);
}


/**@brief Retrieve a list of peer manager peer IDs.
 *
 * @param[inout] p_peers   The buffer where to store the list of peer IDs.
 * @param[inout] p_size    In: The size of the @p p_peers buffer.
 *                         Out: The number of peers copied in the buffer.
 */
static void peer_list_get(pm_peer_id_t * p_peers, uint32_t * p_size)
{
	pm_peer_id_t peer_id;
	uint32_t     peers_to_copy;

	peers_to_copy = (*p_size < BLE_GAP_WHITELIST_ADDR_MAX_COUNT) ?
			*p_size : BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

	peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
	*p_size = 0;

	while ((peer_id != PM_PEER_ID_INVALID) && (peers_to_copy--))
	{
		p_peers[(*p_size)++] = peer_id;
		peer_id = pm_next_peer_id_get(peer_id);
	}
}


static void whitelist_load()
{
	ret_code_t   ret;
	pm_peer_id_t peers[8];
	uint32_t     peer_cnt;

	memset(peers, PM_PEER_ID_INVALID, sizeof(peers));
	peer_cnt = (sizeof(peers) / sizeof(pm_peer_id_t));

	// Load all peers from flash and whitelist them.
	peer_list_get(peers, &peer_cnt);

	ret = pm_whitelist_set(peers, peer_cnt);
	APP_ERROR_CHECK(ret);

	// Setup the device identies list.
	// Some SoftDevices do not support this feature.
	ret = pm_device_identities_list_set(peers, peer_cnt);
	if (ret != NRF_ERROR_NOT_SUPPORTED)
	{
		APP_ERROR_CHECK(ret);
	}
}


/**@brief Function to start scanning.
 */
static void scan_init(void)
{
	if (nrf_fstorage_is_busy(NULL))
	{
		m_memory_access_in_progress = true;
		return;
	}

	// Whitelist buffers.
	ble_gap_addr_t whitelist_addrs[8];
	ble_gap_irk_t  whitelist_irks[8];

	memset(whitelist_addrs, 0x00, sizeof(whitelist_addrs));
	memset(whitelist_irks,  0x00, sizeof(whitelist_irks));

	uint32_t addr_cnt = (sizeof(whitelist_addrs) / sizeof(ble_gap_addr_t));
	uint32_t irk_cnt  = (sizeof(whitelist_irks)  / sizeof(ble_gap_irk_t));

	// Reload the whitelist and whitelist all peers.
	whitelist_load();

	// Get the whitelist previously set using pm_whitelist_set().
	ret_code_t ret = pm_whitelist_get(whitelist_addrs, &addr_cnt,
			whitelist_irks,  &irk_cnt);
	APP_ERROR_CHECK(ret);

	m_scan_param.active   = 0;
	m_scan_param.interval = SCAN_INTERVAL;
	m_scan_param.window   = SCAN_WINDOW;

	if (((addr_cnt == 0) && (irk_cnt == 0)) ||
			(m_whitelist_disabled))
	{
		// Don't use whitelist.
#if (NRF_SD_BLE_API_VERSION == 5)
		m_scan_param.use_whitelist  = 0;
		m_scan_param.adv_dir_report = 0;
#endif

		m_scan_param.timeout  = 0x0000; // No timeout.
	}
	else
	{
		// Use whitelist.

#if (NRF_SD_BLE_API_VERSION == 5)
		m_scan_param.use_whitelist  = 1;
		m_scan_param.adv_dir_report = 0;
#endif

		m_scan_param.timeout  = 0x001E; // 30 seconds.
	}

}

/**@brief Function to start scanning.
 */
static void scan_start(void)
{
	LOG_INFO("Starting scan.");

	uint32_t ret = sd_ble_gap_scan_start(&m_scan_param);
	APP_ERROR_CHECK(ret);

}


/**@brief GATT module event handler.
 */
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
	switch (p_evt->evt_id)
	{
	case NRF_BLE_GATT_EVT_ATT_MTU_UPDATED:
	{
        LOG_INFO("ATT MTU exchange completed.");
	} break;

	case NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED:
	{
		LOG_INFO("Data length for connection 0x%x updated to %d.",
				p_evt->conn_handle,
				p_evt->params.data_length);
	} break;

	default:
		break;
	}

	if (m_retry_db_disc)
	{
		NRF_LOG_DEBUG("Retrying DB discovery.");

		m_retry_db_disc = false;

		// Discover peer's services.
		ret_code_t err_code;
		err_code = ble_db_discovery_start(&m_db_disc, m_pending_db_disc_conn);

		if (err_code == NRF_ERROR_BUSY)
		{
			NRF_LOG_DEBUG("ble_db_discovery_start() returned busy, will retry later.");
			m_retry_db_disc = true;
		}
		else
		{
			APP_ERROR_CHECK(err_code);
		}
	}
}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
	ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
	APP_ERROR_CHECK(err_code);
}


void ble_get_navigation(sKomootNavigation *nav) {

	ASSERT(nav);

	if (m_komoot_nav.isUpdated) memcpy(nav, &m_komoot_nav, sizeof(m_komoot_nav));

}


#ifdef BLE_STACK_SUPPORT_REQD
/**
 * Init BLE stack
 */
void ble_init(void)
{
	ble_stack_init();

	peer_manager_init();

	gatt_init();
	db_discovery_init();

	lns_c_init();
	nus_c_init();
	komoot_c_init();
	scan_init();

	// Start scanning for peripherals and initiate connection
	// with devices
	scan_start();
}

/**
 * Send the log file to a remote computer
 */
#include "sd_functions.h"
void ble_nus_tasks(void) {

	if (m_nus_xfer_state == eNusTransferStateIdle) {

		char c = RING_BUFF_GET_ELEM(nus_rb1);
		RING_BUFFER_POP(nus_rb1);

		model_input_virtual_uart(c);

		return;
	}

	switch (m_nus_xfer_state) {

	case eNusTransferStateInit:
	{
		if (!log_file_start()) {
			NRF_LOG_WARNING("Log file error start")
			m_nus_xfer_state = eNusTransferStateIdle;
		} else {
			m_nus_packet_nb = 0;
			m_nus_cts = true;
			m_nus_xfer_state = eNusTransferStateRun;
		}
	}
	break;

	case eNusTransferStateRun:
	break;

	case eNusTransferStateFinish:
		if (!log_file_stop(false)) {
			NRF_LOG_WARNING("Log file error stop")
		}
		m_nus_xfer_state = eNusTransferStateIdle;
		break;

	default:
		break;
	}

	if (m_connected &&
			m_nus_xfer_state == eNusTransferStateRun &&
			m_nus_cts) {

		char *p_xfer_str = NULL;
		size_t length_ = 0;
		p_xfer_str = log_file_read(&length_);
		if (!p_xfer_str || !length_) {
			// problem or end of transfer
			m_nus_xfer_state = eNusTransferStateFinish;
			return;
		} else {
			// nothing
		}

		uint32_t err_code = ble_nus_c_string_send(&m_ble_nus_c, (uint8_t *)p_xfer_str, length_);

		switch (err_code) {
		case NRF_ERROR_BUSY:
			NRF_LOG_INFO("NUS BUSY");
			break;

		case NRF_ERROR_RESOURCES:
			NRF_LOG_INFO("NUS RESSSS %u", m_nus_packet_nb);
			m_nus_cts = false;
			break;

		case NRF_ERROR_TIMEOUT:
			NRF_LOG_ERROR("NUS timeout", err_code);
			break;

		case NRF_SUCCESS:
			NRF_LOG_INFO("Packet %u sent size %u", m_nus_packet_nb, length_);
			m_nus_packet_nb++;
			break;

		default:
			NRF_LOG_ERROR("NUS unknown error: 0x%X", err_code);
			break;
		}

	}

}


#endif


