/*
 * Points.cpp
 *
 *  Created on: 15 oct. 2017
 *      Author: Vincent
 */

#include <math.h>
#include "Points.h"


Location::Location() {
	_lat = 0;
	_lon = 0;
}

Location::Location(float lat, float lon) {
	_lat = lat;
	_lon = lon;
}

Location::Location(const Location& pt) {
	_lat = pt._lat;
	_lon = pt._lon;
}

Location::Location(const Location* pt) {
	if (pt) {
		_lat = pt->_lat;
		_lon = pt->_lon;
	}
}

Location::~Location() {
}

int Location::isValid() {
	if (_lat == 0. || _lon == 0. || fabsf(_lat) > 89. || fabsf(_lon) > 189.) {
		return 0;
	} else {
		return 1;
	}
}


Point2D::Point2D() {
	_lat = 0.;
	_lon = 0.;
}

Point2D::Point2D(const Point2D& pt) {
	_lat = pt._lat;
	_lon = pt._lon;

	this->increaseCount2D();
}

Point2D::Point2D(const Point2D* pt) {
	if (pt) {
		_lat = pt->_lat;
		_lon = pt->_lon;
	}

	this->increaseCount2D();
}

Point2D::Point2D(const Location& pt) {
	_lat = pt._lat;
	_lon = pt._lon;

	this->increaseCount2D();
}

Point2D::Point2D(const Location* pt) {
	if (pt) {
		_lat = pt->_lat;
		_lon = pt->_lon;
	}

	this->increaseCount2D();
}

Point2D::Point2D(float lat, float lon) {
	_lat = lat;
	_lon = lon;

	this->increaseCount2D();
}

Point2D::~Point2D() {
	this->decreaseCount2D();
}

Point2D& Point2D::operator =(const Location& pt) {
	_lat = pt._lat;
	_lon = pt._lon;
	return *this;
}

Point2D& Point2D::operator =(const Location* pt) {
	_lat = pt->_lat;
	_lon = pt->_lon;
	return *this;
}

void Point2D::increaseCount2D(void) {
	objectCount2D++;
}

void Point2D::decreaseCount2D(void) {
	objectCount2D--;
}

Point::Point() : Location() {
	_alt = 0;
	_rtime = 0;
}

Point::Point(float lat, float lon, float alt, float rtime) {
	_lat = lat;
	_lon = lon;
	_alt = alt;
	_rtime = rtime;

	this->increaseCount();
}

Point::Point(const Location& pt) {
	_lat = pt._lat;
	_lon = pt._lon;
	_alt = 0.;
	_rtime = 0.;

	this->increaseCount();
}

Point::Point(const Location* pt) {
	if (pt) {
		_lat = pt->_lat;
		_lon = pt->_lon;
		_alt = 0.;
		_rtime = 0.;
	}

	this->increaseCount();
}

Point::Point(const Point& pt) {
	_lat = pt._lat;
	_lon = pt._lon;
	_alt = pt._alt;
	_rtime = pt._rtime;

	this->increaseCount();
}

Point::Point(const Point* pt) {
	if (pt) {
		_lat = pt->_lat;
		_lon = pt->_lon;
		_alt = pt->_alt;
		_rtime = pt->_rtime;
	}

	this->increaseCount();
}

Point::~Point() {
	this->decreaseCount();
}

void Point::increaseCount(void) {
	objectCount++;
}

void Point::decreaseCount(void) {
	objectCount--;
}

Point & Point::operator=(const Point *point) {
	if (point != 0) {
		_lat = point->_lat;
		_lon = point->_lon;
		_alt = point->_alt;
		_rtime = point->_rtime;
		return *this;
	} else {
		return *this;
	}
}

Point & Point::operator=(const Point &point) {
	_lat = point._lat;
	_lon = point._lon;
	_alt = point._alt;
	_rtime = point._rtime;
	return *this;
}

Point& Point::operator =(const Location& point) {
	_lat = point._lat;
	_lon = point._lon;
	return *this;
}

Point& Point::operator =(const Location* point) {
	_lat = point->_lat;
	_lon = point->_lon;
	return *this;
}

void Point::toString() {
	//printf("Point -> %f %f %f %f\n", _lat, _lon, _alt, _rtime);
}
