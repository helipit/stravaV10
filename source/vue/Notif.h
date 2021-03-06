/*
 * Notif.h
 *
 *  Created on: 12 dec. 2017
 *      Author: Vincent
 */

#ifndef SOURCE_VUE_NOTIF_H_
#define SOURCE_VUE_NOTIF_H_

#include <list>
#include <stdint.h>
#include "notifications.h"
#include "WString.h"


class Notif {
public:
	Notif(const char *title_, const char *msg_, uint8_t persist = 5, eNotificationType type_ = eNotificationTypePartial) {
		m_persist = persist;
		m_type    = type_;
		m_title   = title_;
		m_msg     = msg_;
	}

	uint8_t m_persist;
	eNotificationType m_type;
	String m_title;
	String m_msg;
};


class NotifiableDevice {
public:
	NotifiableDevice() {}
	~NotifiableDevice() {
		m_notifs.clear();
	}

	/**
	 *
	 * @param title_
	 * @param msg_
	 * @param persist_
	 * @param type_ If partial, only the msg is printed
	 */
	void addNotif(const char *title_, const char *msg_, uint8_t persist_ = 5, eNotificationType type_ = eNotificationTypePartial) {
		if (m_notifs.size() < 10) m_notifs.push_back(Notif(title_, msg_, persist_, type_));
	}

protected:
	std::list<Notif> m_notifs;
};

#endif /* SOURCE_VUE_NOTIF_H_ */
