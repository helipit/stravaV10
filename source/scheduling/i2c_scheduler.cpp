/*
 * i2c_scheduler.c
 *
 *  Created on: 10 déc. 2017
 *      Author: Vincent
 */

#include "i2c.h"
#include <i2c_scheduler.h>
#include "segger_wrapper.h"
#include "parameters.h"
#include "millis.h"
#include "millis.h"
#include "fxos.h"
#include "fram.h"
#include "app_timer.h"
#include "Model.h"



#ifndef _DEBUG_TWI

#define I2C_SCHEDULING_PERIOD_MS      (BARO_REFRESH_PER_MS)

static uint32_t m_last_polled_index = 0;

APP_TIMER_DEF(m_timer);


/**
 *
 */
static void _i2c_scheduling_sensors_post_init(void) {

	LOG_WARNING("Sensors initialized");
}


/**
 *
 */
static void _i2c_scheduling_sensors_init() {

	// Init sensors configuration
	fxos_init();

#ifdef VEML_PRESENT
	veml.init();
#endif

	// init configuration
	stc.init(STC3100_CUR_SENS_RES_MO);

#ifdef FRAM_PRESENT
	fram_init_sensor();
#endif
  
	baro.sensorInit();

	// post-init steps
	_i2c_scheduling_sensors_post_init();
}

/**
 *
 * @param p_context
 */
static void timer_handler(void * p_context)
{
	W_SYSVIEW_RecordEnterISR();

	if (boucle__get_mode() == eBoucleGlobalModesCRS ||
			boucle__get_mode() == eBoucleGlobalModesPRC) {
		baro.sensorRead();
	} else {
		//baro.sleep();
	}

	if (++m_last_polled_index >= SENSORS_REFRESH_PER_MS / I2C_SCHEDULING_PERIOD_MS) {
		m_last_polled_index = 0;

		if (boucle__get_mode() == eBoucleGlobalModesCRS ||
				boucle__get_mode() == eBoucleGlobalModesPRC) {
			//baro.sensorRead();
		} else {
			baro.sleep();
		}

		stc.readChip();

#ifdef VEML_PRESENT
		veml.readChip();
#endif

	}

    W_SYSVIEW_RecordExitISR();
}

#endif

/**
 *
 */
void i2c_scheduling_init(void) {
#ifndef _DEBUG_TWI

	_i2c_scheduling_sensors_init();

	delay_ms(3);

	ret_code_t err_code;
	err_code = app_timer_create(&m_timer, APP_TIMER_MODE_REPEATED, timer_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_start(m_timer, APP_TIMER_TICKS(I2C_SCHEDULING_PERIOD_MS), NULL);
	APP_ERROR_CHECK(err_code);

#else
    stc.init(100);
    veml.init();
    baro.init();

    if (fxos_init()) LOG_ERROR("FXOS init fail");
#endif
}

void i2c_scheduling_tasks(void) {
#ifndef _DEBUG_TWI
	if (baro.isUpdated()) {
		sysview_task_void_enter(I2cMgmtReadMs);
		baro.sensorRefresh();
		sysview_task_void_exit(I2cMgmtReadMs);
	}
#ifdef VEML_PRESENT
	if (is_veml_updated()) {
		sysview_task_void_enter(I2cMgmtRead2);
		veml.refresh();
		sysview_task_void_exit(I2cMgmtRead2);
	}
#endif
	if (is_stc_updated()) {
		sysview_task_void_enter(I2cMgmtRead2);
		stc.refresh();
		sysview_task_void_exit(I2cMgmtRead2);
	}
#else
	static uint32_t _counter = 0;

	if (++_counter >= SENSORS_REFRESH_PER_MS / APP_TIMEOUT_DELAY_MS) {
		_counter = 0;
		stc.refresh(nullptr);
#ifdef VEML_PRESENT
		veml.refresh(nullptr);
#endif
		if (boucle.getGlobalMode() != eBoucleGlobalModesFEC) fxos_tasks(nullptr);
		if (boucle.getGlobalMode() != eBoucleGlobalModesFEC) baro.refresh(nullptr);
	}
#endif

	// dispatch to model
	model_dispatch_sensors_update();
}
