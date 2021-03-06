/*
 * Vue.cpp
 *
 *  Created on: 12 d�c. 2017
 *      Author: Vincent
 */

#include <vue/Vue.h>
#include "millis.h"
#include "Model.h"
#include "nordic_common.h"
#include "assert_wrapper.h"
#include "WString.h"
#include "segger_wrapper.h"
#include "Org_01.h"

Vue::Vue() : Adafruit_GFX(LS027_HW_WIDTH, LS027_HW_HEIGHT) {

	m_global_mode = VUE_DEFAULT_MODE;
	m_last_refreshed = 0;

}

void Vue::init(void) {
	this->setRotation(3);
	this->initMenu();
	this->setTextWrap(false);
	this->setFont(&Org_01);
	LS027_Init();
}

void Vue::tasks(eButtonsEvent event) {

	switch (m_global_mode) {
	case eVueGlobalScreenCRS:
	{
		// propagate to the inner vue
		if (!m_is_menu_selected) {
			this->propagateEventsCRS(event);
		}
		this->propagateEvent(event);
		break;
	}
	case eVueGlobalScreenFEC:
	{
		// propagate to the inner vue
		this->propagateEvent(event);
		break;
	}
	case eVueGlobalScreenPRC:
	{
		if (!m_is_menu_selected) {
			this->propagateEventsPRC(event);
		}
		this->propagateEvent(event);
		break;
	}
	case eVueGlobalScreenDEBUG:
	{
		// propagate to the inner menu
		this->propagateEvent(event);
		break;
	}
	default:
		break;
	}

}

void Vue::setCurrentMode(eVueGlobalScreenModes mode_) {

	m_global_mode = mode_;

}

void Vue::refresh(void) {

	m_last_refreshed = millis();

	sysview_task_void_enter(Ls027Clear);
	this->clearDisplay();
	sysview_task_void_exit(Ls027Clear);

	if (m_is_menu_selected) {
		this->tasksMenu();
	} else {
		switch (m_global_mode) {
		case eVueGlobalScreenCRS:
			this->tasksCRS();
			break;
		case eVueGlobalScreenFEC:
			this->tasksFEC();
			break;
		case eVueGlobalScreenPRC:
			this->tasksPRC();
			break;
		case eVueGlobalScreenDEBUG:
			this->displayDebug();
			break;

		default:
			break;
		}
	}

	// Notifications tasks
	if (m_notifs.size()) {

		this->setTextWrap(true);

		Notif& notif = m_notifs.front();

		this->setCursor(5, 5);
		this->setTextSize(2);

		String text;

		if (eNotificationTypeComplete == notif.m_type) {
			text = notif.m_title;
			text += ": ";
		}

		text += notif.m_msg;

		int16_t x1=0; int16_t y1=0; uint16_t w=0; uint16_t h=0;
		if (text > 2)
			this->getTextBounds((char*)text.c_str(), this->getCursorX(), this->getCursorY(), &x1, &y1, &w, &h);

		h += this->getCursorY() + 15;
		this->drawFastHLine(0, h, _width, textcolor);
		this->fillRect(0, 0, _width, h, textbgcolor);
		this->print(text);

		if (notif.m_persist-- == 0) {
			m_notifs.pop_front();
		}

		this->setTextWrap(false);
	}

}

void Vue::drawPixel(int16_t x, int16_t y, uint16_t color) {

	if((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

//  always 3 here, comment for speedup

//	switch(rotation) {
//	case 1:
//		adagfxswap(x, y);
//		x = WIDTH  - 1 - x;
//		break;
//	case 2:
//		x = WIDTH  - 1 - x;
//		y = HEIGHT - 1 - y;
//		break;
//	case 3:
//		adagfxswap(x, y);
//		y = HEIGHT - 1 - y;
//		break;
//	}

	LS027_drawPixel(y, HEIGHT - 1 - x, color);
}

void Vue::drawPixelGroup(int16_t x, int16_t y, uint16_t nb, uint16_t color) {

	if((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;

//  always 3 here, comment for speedup

//	switch(rotation) {
//	case 1:
//		adagfxswap(x, y);
//		x = WIDTH  - 1 - x;
//		break;
//	case 2:
//		x = WIDTH  - 1 - x;
//		y = HEIGHT - 1 - y;
//		break;
//	case 3:
//		adagfxswap(x, y);
//		y = HEIGHT - 1 - y;
//		break;
//	}

	LS027_drawPixelGroup(y, HEIGHT - 1 - x, nb, color);
}

// Fill a rounded rectangle
void Vue::fillRoundRect(int16_t x, int16_t y, int16_t w,
		int16_t h, int16_t r, uint16_t color) {

	// smarter version
	for (int i=1; i <= r; i++) {

		int c_port = r-floorSqrt(r*r - i*i);

		drawFastVLine(x-i       , y+c_port , h-2*c_port, color); // Left
		drawFastVLine(x+w-2*r+i , y+c_port , h-2*c_port, color); // Right

	}

	fillRect(x, y , w-2*r+1, h, color);

}

void Vue::drawFastVLine(int16_t x, int16_t y,
		int16_t h, uint16_t color) {

	// this method is here sped up greatly !
	drawPixelGroup(x, y, h, color);

}

void Vue::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
		uint16_t color) {
	// we make sure to here use drawFastVLine which is way faster than drawFastHLine
	for (int16_t i=x; i<x+w; i++) {
		drawFastVLine(i, y, h, color);
	}
}

void Vue::cadranH(uint8_t p_lig, uint8_t nb_lig, const char *champ, String  affi, const char *p_unite) {

	sysview_task_void_enter(Ls027Cadrans);

	int decal = 0;
	int x = _width / 4;
	int y = _height / nb_lig * (p_lig - 1);

	setCursor(5, y + 8);
	setTextSize(1);

	if (champ) print(champ);

	if (affi.length() > 9) {
		affi = "-----";
	} else {
		decal = (4 - affi.length()) * 6;
	}

	setCursor(x + 20 + decal, y - 10 + (_height / (nb_lig*2)));
	setTextSize(3);
	print(affi);

	setTextSize(1);
	x = _width / 2;
	setCursor(x + 105, y + 8);// y + 42

	if (p_unite) print(p_unite);

	// print delimiters
	if (p_lig > 1) drawFastHLine(0, _height / nb_lig * (p_lig - 1), _width, 1);
	if (p_lig < nb_lig) drawFastHLine(0, _height / nb_lig * (p_lig), _width, 1);

	sysview_task_void_exit(Ls027Cadrans);

	w_task_yield();
}


void Vue::cadran(uint8_t p_lig, uint8_t nb_lig, uint8_t p_col, const char *champ, String  affi, const char *p_unite) {

	sysview_task_void_enter(Ls027Cadrans);

	const int x = _width / 2 * p_col;
	const int y = _height / nb_lig * (p_lig - 1);

	if (champ) {
		setCursor(x + 5 - _width / 2, y + 8);
		setTextSize(1);
		print(champ);
	}

	const int len = affi.length();

	if (len > 6) {
		affi = "---";
	}
//	else if(len > 4) {
//		decal += 20;
//	}

	setTextSize(3);

	int16_t x1; int16_t y1; uint16_t w=40; uint16_t h;
	getTextBounds((char*)affi.c_str(), 0, 0, &x1, &y1, &w, &h);

	setCursor(x - 55 + w/2, y - 10 + (_height / (nb_lig*2)));
	//drawRect (x - 60 + w/2, y - 10 + (_height / (nb_lig*2)), -w, h, 1);

	printRev(affi);

	setTextSize(1);
	setCursor(x - 4, y + 8);

	if (p_unite) printRev(p_unite);

	// print delimiters
	drawFastVLine(_width / 2, _height / nb_lig * (p_lig - 1), _height / nb_lig, 1);

	if (p_lig > 1)      drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * (p_lig - 1), _width / 2, 1);
	if (p_lig < nb_lig) drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * p_lig, _width / 2, 1);

	sysview_task_void_exit(Ls027Cadrans);

	w_task_yield();
}

void Vue::HistoH(uint8_t p_lig, uint8_t nb_lig, sVueHistoConfiguration& h_config_) {

	const int y_base = _height / nb_lig * (p_lig);

	if (h_config_.cur_elem_nb) {

		ASSERT(h_config_.p_f_read);
		ASSERT(h_config_.nb_elem_tot);

		uint16_t dx = _width / h_config_.nb_elem_tot;

		// iterate in buffer
		for (int i=0; i < h_config_.cur_elem_nb; i++) {

			int x_base0 = i * dx;

			tHistoValue elem   = (h_config_.p_f_read)(i);
			tHistoValue height = elem * _height / (nb_lig * h_config_.max_value);

			uint16_t dy = 10;

			if (h_config_.ref_value) {

				if (elem <= h_config_.ref_value) height = h_config_.ref_value * _height / (nb_lig * h_config_.max_value);
				else                             height -= 1;

				dy = abs(elem - h_config_.ref_value) * _height / (nb_lig * h_config_.max_value);

				dy = MAX(dy, 1);

				drawFastPHLine(0, y_base - h_config_.ref_value * _height / (nb_lig * h_config_.max_value), _width, 1);
			}

			// rectangle complet
			this->fillRect(x_base0, y_base-height, dx, dy, 1);
		}
	}

	// print delimiters
	if (p_lig > 1)      drawFastHLine(0, _height / nb_lig * (p_lig - 1), _width, 1);
	if (p_lig < nb_lig) drawFastHLine(0, _height / nb_lig * (p_lig), _width, 1);
}

void Vue::Histo(uint8_t p_lig, uint8_t nb_lig, uint8_t p_col, sVueHistoConfiguration& h_config_) {

	int x_base = _width / 2 * (p_col - 1);
	int y_base = _height / nb_lig * (p_lig);

	if (h_config_.cur_elem_nb) {

		ASSERT(h_config_.p_f_read);
		ASSERT(h_config_.nb_elem_tot);

		int dx = _width / (2 * h_config_.nb_elem_tot);

		// iterate in buffer
		for (int i=0; i < h_config_.cur_elem_nb; i++) {

			int x_base0 = x_base + i * dx;

			tHistoValue elem   = (h_config_.p_f_read)(i);
			tHistoValue height = elem * _height / (nb_lig * h_config_.max_value);

//			// rectangle complet
//			this->fillRect(x_base0, y_base-height, dx, height, 2);
			// petite barre
			this->fillRect(x_base0, y_base-height, dx, 4, 1);
		}
	}

	// print delimiters
	drawFastVLine(_width / 2, _height / nb_lig * (p_lig - 1), _height / nb_lig, 1);

	if (p_lig > 1)      drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * (p_lig - 1), _width / 2, 1);
	if (p_lig < nb_lig) drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * p_lig, _width / 2, 1);
}

void Vue::cadranRR(uint8_t p_lig, uint8_t nb_lig, uint8_t p_col, const char *champ, RRZone &zone) {

	const int x = _width / 2 * p_col;
	const int y = _height / nb_lig * (p_lig - 1);

	if (champ) {
		setCursor(x + 5 - _width / 2, y + 8);
		setTextSize(1);
		print(champ);
	}

	// Print the data
	setCursor(x - 55 - 20, y - 10 + (_height / (nb_lig*2)));
	setTextSize(3);

	uint32_t cur_zone = zone.getCurBin();
	float val_max = zone.getValMax();

	// force span to be in a window
	val_max = MIN(val_max, 50);
	val_max = MAX(val_max, 5);

	// loop over bins
	for (uint32_t i=0; i< zone.getNbBins(); i++) {

		float val = zone.getValZX(i);

		LOG_DEBUG(">> RR val displayed: %f", val);

		int16_t width = regFenLim(val, 0, val_max, 2, _width / 2 - 35);
		this->fillRect(x - _width / 2 + 20, y + 20 + i*6, width, 4, 1);

		if (i == cur_zone) {
			setCursor(x - _width / 2 + 7, y + 15 + i*6);
			setTextSize(2);
			print((char)('>'));
		}

	}

	setCursor(x - _width / 2 + 40, y + 17 + zone.getNbBins()*6);
	setTextSize(2);
	print((unsigned)zone.getValMax());

	// print delimiters
	drawFastVLine(_width / 2, _height / nb_lig * (p_lig - 1), _height / nb_lig, 1);

	if (p_lig > 1)      drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * (p_lig - 1), _width / 2, 1);
	if (p_lig < nb_lig) drawFastHLine(_width * (p_col - 1) / 2, _height / nb_lig * p_lig, _width / 2, 1);

}
