/*
 * Locator.h
 *
 *  Created on: 19 oct. 2017
 *      Author: Vincent
 */

#ifndef SOURCE_MODEL_LOCATOR_H_
#define SOURCE_MODEL_LOCATOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "g_structs.h"

#define MAX_SATELLITES     40
#define ACTIVE_VAL         5

typedef struct {
	float lat;
	float lon;
	float alt;
	float speed;
	float course;
} SLoc;

typedef struct {
	float gps_ele;
	float baro_ele;
	float baro_corr;
	float filt_ele;
	float alpha_bar;
	float alpha_zero;
	float climb;
	float vit_asc;
	float rough[3];
	float b_rough;
} SEle;

typedef struct {
	uint8_t bpm;
	uint32_t cadence;
	int16_t pwr;
} SSensors;

typedef struct {
	uint32_t secj;
	uint32_t date;
	uint32_t timestamp;
} SDate;

typedef struct
{
	int active;
	int elevation;
	int azimuth;
	int snr;
	int no;
} sSatellite;

typedef enum {
	eLocationSourceNone,
	eLocationSourceSIM,
	eLocationSourceNRF,
	eLocationSourceGPS,
} eLocationSource;

typedef struct {
	float lat;
	float lon;
	float alt;
	float speed;
	float course;
	uint32_t utc_time;
	uint32_t utc_timestamp;
	uint32_t date;
} sLocationData;

#if defined(__cplusplus)
extern "C" {
#endif /* _cplusplus */

uint32_t locator_encode_char(char c);

void locator_dispatch_lns_update(sLnsInfo *lns_info);

#if defined(__cplusplus)
}

#include "Sensor.h"
#include "Attitude.h"

/**
 *
 */
class Locator {
public:
	Locator();
	void init();
	void tasks();

	void displayGPS2(void);

	bool getGPSDate(int &iYr, int &iMo, int &iDay, int &iHr);

	eLocationSource getDate(SDate& date_);
	eLocationSource getPosition(SLoc& loc_, SDate& date_);

	eLocationSource getUpdateSource();

	bool isUpdated();

	uint32_t getLastUpdateAge();
	uint32_t getUsedSatsAge();

	Sensor<sLocationData> nrf_loc;
	Sensor<sLocationData> sim_loc;
	Sensor<sLocationData> gps_loc;

private:
	bool anyChanges;

	uint16_t m_nb_nrf_pos;
	uint16_t m_nb_sats;
};

#endif /* _cplusplus */
#endif /* SOURCE_MODEL_LOCATOR_H_ */
