/*
 * GPS.cpp
 *
 *  Created on: 13 d�c. 2017
 *      Author: Vincent
 */

#include <GPSMGMT.h>
#include <stdio.h>
#include "MTK.h"
#include "uart_tdd.h"
#include "utils.h"
#include "millis.h"
#include "WString.h"
#include "LocusCommands.h"
#include "Screenutils.h"
#include "sd_functions.h"
#include "Locator.h"
#include "Model.h"
#include "parameters.h"
#include "segger_wrapper.h"


#define GPS_DEFAULT_SPEED_BAUD     NRF_UARTE_BAUDRATE_9600
#define GPS_FAST_SPEED_BAUD        NRF_UARTE_BAUDRATE_115200

#if (GPS_FAST_SPEED_BAUD > GPS_DEFAULT_SPEED_BAUD)
#define GPS_BIN_CMD                PMTK_SET_BIN
#define GPS_NMEA_CMD               PMTK_SET_NMEA_BAUD_115200
#else
#define GPS_BIN_CMD                PMTK_SET_BIN
#define GPS_NMEA_CMD               PMTK_SET_NMEA_BAUD_9600
#endif

#define GPS_UART_SEND(X,Y) do { \
		uart_send(X, Y); } while(0)

#define SEND_TO_GPS(X) do { \
		const_char_to_buffer(X, buffer, sizeof(buffer)); \
		GPS_UART_SEND(buffer, strlen(X)); } while(0)


static uint8_t buffer[256];

static eGPSMgmtTransType  m_trans_type = eGPSMgmtTransNMEA;
static eGPSMgmtEPOState   m_epo_state  = eGPSMgmtEPOIdle;

static MTK m_rec_packet;

static bool m_is_uart_on = false;

static nrf_uarte_baudrate_t m_uart_baud = GPS_DEFAULT_SPEED_BAUD;


/**
 *
 */
void gps_uart_start() {

	m_uart_baud = GPS_DEFAULT_SPEED_BAUD;
	uart_timer_init();
	uart_init(m_uart_baud);
	m_is_uart_on = true;

}

void gps_uart_stop() {

	if (m_is_uart_on) uart_uninit();
	m_is_uart_on = false;

}

void gps_uart_resume() {

	if (!m_is_uart_on) uart_init(m_uart_baud);
	m_is_uart_on = true;

}

GPS_MGMT::GPS_MGMT() {

	m_power_state = eGPSMgmtPowerOn;
	m_trans_type  = eGPSMgmtTransNMEA;
	m_epo_state   = eGPSMgmtEPOIdle;

}

void GPS_MGMT::init(void) {

	gps_uart_stop();
	gps_uart_start();
	// the baud is here always 9600

	// configure fix pin
	//nrf_gpio_cfg_input(FIX_PIN, NRF_GPIO_PIN_PULLDOWN);

	SEND_TO_GPS(PMTK_AWAKE);
	delay_us(500);

#if GPS_USE_COLD_START
	SEND_TO_GPS(PMTK_COLD);
	delay_us(500);
#endif

	if (GPS_FAST_SPEED_BAUD > GPS_DEFAULT_SPEED_BAUD) {

		// change GPS baudrate
		SEND_TO_GPS(PMTK_SET_NMEA_BAUD_115200);

		// go to final baudrate
		delay_ms(100);
		gps_uart_stop();
		delay_us(500);
		m_uart_baud = GPS_FAST_SPEED_BAUD;
		gps_uart_resume();

	}

}

bool GPS_MGMT::isFix(void) {
	// TODO nrf_gpio_pin_read(FIX_PIN);
	return 1;
}

void GPS_MGMT::standby(void) {

	if (eGPSMgmtPowerOff == m_power_state) return;

	LOG_INFO("GPS put in standby\r\n");

	m_power_state = eGPSMgmtPowerOff;

	SEND_TO_GPS(PMTK_STANDBY);

	delay_ms(10);
	gps_uart_stop();
	delay_us(500);
	m_uart_baud = GPS_DEFAULT_SPEED_BAUD;
	gps_uart_resume();
}

void GPS_MGMT::awake(void) {

	if (eGPSMgmtPowerOn == m_power_state) return;

	LOG_INFO("GPS awoken\r\n");

	m_power_state = eGPSMgmtPowerOn;

	SEND_TO_GPS(PMTK_AWAKE);

}

void GPS_MGMT::startEpoUpdate(void) {

	LOG_INFO("EPO update started\r\n");

	m_epo_state = eGPSMgmtEPOStart;

	this->awake();
}

/**
 *
 * @param result
 */
void GPS_MGMT::getAckResult(const char *result) {

	int int_res = atoi(result);

//	if (int_res == 3) {
//		vue.addNotif("GPSMGMT: ", "Result: success", 4, eNotificationTypeComplete);
//	} else {
//		vue.addNotif("GPSMGMT: ", "Result: failure", 4, eNotificationTypeComplete);
//	}

}

void GPS_MGMT::tasks(void) {

	switch (m_epo_state) {
	case eGPSMgmtEPOIdle:
		break;

	case eGPSMgmtEPOStart:
	{
		m_epo_packet_nb = 0;
		m_epo_packet_ind = 0;

		int size = epo_file_size();
		int res  = epo_file_start();

		if (size <= 0 || res < 0) {
			LOG_ERROR("EPO start failure, size=%ld\r\n", size);

			m_epo_state = eGPSMgmtEPOIdle;

		} else {
			LOG_INFO("EPO size: %ld bytes\r\n", size);

			m_epo_state = eGPSMgmtEPORunning;

			// set transport to binary
			SEND_TO_GPS(GPS_BIN_CMD);

//			vue.addNotif("GPSMGMT: ", "EPO update started", 4, eNotificationTypeComplete);

			delay_ms(500);

			m_trans_type = eGPSMgmtTransBIN;
		}
	}
	break;

	case eGPSMgmtEPORunning:
	{
		// fill the packet
		if (m_epo_packet_ind < 0xFFFF) {

			sEpoPacketBinData epo_data;
			memset(&epo_data, 0, sizeof(epo_data));

			for (int i=0; i < MTK_EPO_MAX_SAT_DATA; i++) {
				// read file
				int ret_code = epo_file_read(&epo_data.sat_data[epo_data.nb_sat]);

				if (ret_code < 0) {
					// error
				} else if (ret_code == 1) {
					// end
				} else {
					epo_data.nb_sat  += 1;
				}
			}

			// prepare the packet to be sent
			if (epo_data.nb_sat > 0) {
				epo_data.epo_seq = m_epo_packet_ind++;

				LOG_INFO("EPO sending packet #%u - %u sats\r\n",
						epo_data.epo_seq, epo_data.nb_sat);

				LOG_DEBUG("Sat data 1: %X %X\r\n",
						epo_data.sat_data[0].sat[0],
						epo_data.sat_data[0].sat[1]);

				MTK tmp_mtk(&epo_data);
				tmp_mtk.toBuffer(buffer, sizeof(buffer));

				GPS_UART_SEND(buffer, tmp_mtk.getPacketLength());
			} else {
				// process is finished
				m_epo_packet_ind = 0xFFFF;

				epo_data.epo_seq = m_epo_packet_ind;

				LOG_INFO("EPO last packet\r\n");

				MTK tmp_mtk(&epo_data);
				tmp_mtk.toBuffer(buffer, sizeof(buffer));

				GPS_UART_SEND(buffer, tmp_mtk.getPacketLength());
			}

			m_epo_state = eGPSMgmtEPOWaitForEvent;
		}
	}
	break;

	case eGPSMgmtEPOWaitForEvent:
	{
	}
	break;

	case eGPSMgmtEPOEnd:
	{
		LOG_INFO("EPO update end\r\n");

//		vue.addNotif("GPSMGMT: ", "EPO update success", 5, eNotificationTypeComplete);

		// the file should be deleted only upon success
		(void)epo_file_stop(m_epo_packet_ind == 0xFFFF);

		// NMEA + baud rate default
		uint8_t cmd[5] = {0x00,0x00,0x00,0x00,0x00};
		encode_uint32 (cmd + 1, GPS_FAST_SPEED_BAUD);

		// set transport to NMEA
		MTK tmp_mtk(MTK_FMT_NMEA_CMD_ID, cmd, sizeof(cmd));
		tmp_mtk.toBuffer(buffer, sizeof(buffer));

		LOG_INFO("MTK switched to NMEA\r\n");

		GPS_UART_SEND(buffer, tmp_mtk.getPacketLength());

		m_epo_state = eGPSMgmtEPOIdle;

	}
	break;

	default:
		break;
	}
}

/**
 *
 * @param c Input character
 * @return Error code
 */
uint32_t gps_encode_char(char c) {

	//LOG_INFO("%c", c);
	//LOG_FLUSH();

	if (eGPSMgmtTransNMEA == m_trans_type) {

		locator_encode_char(c);

	} else {

		if (m_rec_packet.encode(c)) {

			// the packet is valid
			switch (m_rec_packet.getCommandId()) {
			case MTK_EPO_BIN_ACK_CMD_ID:
				if (eGPSMgmtEPOWaitForEvent == m_epo_state) {

					if (m_rec_packet.m_packet.raw_packet.data[2] == 0x01) {
						LOG_INFO("Packet ack recv\r\n");

						uint16_t epo_seq = decode_uint16(m_rec_packet.m_packet.raw_packet.data);

						// resume sending packets
						if (epo_seq < 0xFFFF) m_epo_state = eGPSMgmtEPORunning;
						else                  m_epo_state = eGPSMgmtEPOEnd;

					} else {
						LOG_ERROR("Packet ack recv with error %X\r\n",
								m_rec_packet.m_packet.raw_packet.data[2]);

						// end it
						m_epo_state = eGPSMgmtEPOEnd;
					}
				}
				break;

			case MTK_ACK_CMD_ID:
			{
				uint16_t cmd_id = decode_uint16(m_rec_packet.m_packet.raw_packet.data);
				uint8_t status  = m_rec_packet.m_packet.raw_packet.data[2];
				LOG_INFO("Binary ACK to cmd 0x%X with status=%u\r\n", cmd_id, status);

				if (MTK_FMT_NMEA_CMD_ID == cmd_id &&
						status == 3) {
					m_trans_type = eGPSMgmtTransNMEA;
				}
			}
			break;

			case MTK_EMPTY_CMD_ID:
				break;

			default:
				break;
			}

			// reset packet
			m_rec_packet.clear();
		}

	}

	return 0;
}

/**
 *
 * @param loc_data
 * @param age_
 */
void GPS_MGMT::startHostAidingEPO(sLocationData& loc_data, uint32_t age_) {

	String _lat = _fmkstr(loc_data.lat, 6U);
	String _lon = _fmkstr(loc_data.lon, 6U);
	String _alt = _fmkstr(loc_data.alt, 0U);

	LOG_INFO("Host aiding alt: %d", (int)loc_data.alt);

	uint16_t _year  = 2000 + (loc_data.date % 100);
	uint16_t _month = (loc_data.date / 100) % 100;
	uint16_t _day   = (loc_data.date / 10000) % 100;

	String _time = _secjmkstr(loc_data.utc_time + (age_ / 1000), ',');

	String cmd = "$PMTK741," + _lat + "," + _lon;
	cmd += "," + _alt;
	cmd += "," + String(_year) + "," + String(_month) + "," + String(_day);
	cmd += "," + _time;

	memset(buffer, 0, sizeof(buffer));

	cmd.toCharArray((char*)buffer, sizeof(buffer), 0);

	// handle checksum
	uint8_t ret = 0;
	for (uint16_t i = 1; i < cmd.length(); i++) {
		ret ^= buffer[i];
	}

	snprintf((char*)buffer + cmd.length(), sizeof(buffer) - cmd.length(), "*%02X\r\n", ret);

	GPS_UART_SEND(buffer, cmd.length() + 5);

	LOG_INFO("Host aiding: %s", (char*)buffer);

//	vue.addNotif("EPO", "Host aiding sent", 5, eNotificationTypeComplete);
}

/**
 *
 * @param interval
 */
void GPS_MGMT::setFixInterval(uint16_t interval) {

	String cmd = "$PMTK220," + interval;

	memset(buffer, 0, sizeof(buffer));

	cmd.toCharArray((char*)buffer, sizeof(buffer), 0);

	// handle checksum
	uint8_t ret = 0;
	for (uint16_t i = 1; i < cmd.length(); i++) {
		ret ^= buffer[i];
	}

	snprintf((char*)buffer + cmd.length(), sizeof(buffer) - cmd.length(), "*%02X\r\n", ret);

	GPS_UART_SEND(buffer, cmd.length() + 5);

}
