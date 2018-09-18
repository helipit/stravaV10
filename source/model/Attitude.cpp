/*
 * Attitude.cpp
 *
 *  Created on: 29 d�c. 2017
 *      Author: Vincent
 */

#include <Attitude.h>
#include "Model.h"
#include "sd_functions.h"


Attitude::Attitude() {
	m_last_save_dist = 0.;

	m_st_buffer_nb_elem = 0;

	m_is_init = false;
}


/**
 *
 * @param loc_
 */
void Attitude::addNewDate(SDate &date_) {

	// update date
	memcpy(&att.date, &date_, sizeof(SDate));
}

/**
 *
 * @param loc_
 */
void Attitude::addNewLocation(SLoc& loc_, SDate &date_) {

	// small correction to allow time with millisecond precision
	float cur_time = (float)date_.secj + ((millis() - date_.timestamp) / 1000.);

	// add this point to our historic
	mes_points.ajouteFinIso(loc_.lat, loc_.lon, loc_.alt, cur_time, HISTO_POINT_SIZE);

	float tmp_dist = distance_between(att.loc.lat, att.loc.lon, loc_.lat, loc_.lon);

	att.dist += tmp_dist;

	if (m_is_init &&
			att.dist > m_last_save_dist + 15) {

		m_last_save_dist = att.dist;

		// save position on queue
		if (m_st_buffer_nb_elem >= ATT_BUFFER_NB_ELEM) {

			// save on SD
			sd_save_pos_buffer(m_st_buffer, ATT_BUFFER_NB_ELEM);

			m_st_buffer_nb_elem = 0;

		}

		// save on buffer
		memcpy(&m_st_buffer[m_st_buffer_nb_elem].loc , &loc_ , sizeof(SLoc));
		memcpy(&m_st_buffer[m_st_buffer_nb_elem].date, &date_, sizeof(SDate));
		m_st_buffer[m_st_buffer_nb_elem].pwr = att.pwr;

		m_st_buffer_nb_elem++;

	} else if (att.dist > m_last_save_dist + 25) {
		// first position
		att.dist = 0;
		m_is_init = true;
	}

	// update global location
	memcpy(&att.loc , &loc_ , sizeof(SLoc));
	// update date
	memcpy(&att.date, &date_, sizeof(SDate));
}


/**
 *
 */
#ifdef ANT_STACK_SUPPORT_REQD
void Attitude::addNewFECPoint(sFecInfo& fec_) {

	// TODO

}
#endif
