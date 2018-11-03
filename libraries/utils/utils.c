/* 
 * File:   utils.c
 * Author: vincent
 *
 * Created on October 27, 2015, 10:55 AM
 */


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "app_util_platform.h"
#include "arm_math.h"
#include "utils.h"
#include "segger_wrapper.h"

#ifndef _USE_MATH_DEFINES

#define M_E		2.7182818284590452354
#define M_LOG2E		1.4426950408889634074
#define M_LOG10E	0.43429448190325182765
#define M_LN2		_M_LN2
#define M_LN10		2.30258509299404568402
#define M_PI		3.14159265358979323846
#define M_TWOPI         (M_PI * 2.0)
#define M_PI_2		1.57079632679489661923
#define M_PI_4		0.78539816339744830962
#define M_3PI_4		2.3561944901923448370E0
#define M_SQRTPI        1.77245385090551602792981
#define M_1_PI		0.31830988618379067154
#define M_2_PI		0.63661977236758134308
#define M_2_SQRTPI	1.12837916709551257390
#define M_SQRT2		1.41421356237309504880
#define M_SQRT1_2	0.70710678118654752440
#define M_LN2LO         1.9082149292705877000E-10
#define M_LN2HI         6.9314718036912381649E-1
#define M_SQRT3	1.73205080756887719000
#define M_IVLN10        0.43429448190325182765 /* 1 / log(10) */
#define M_LOG2_E        _M_LN2
#define M_INVLN2        1.4426950408889633870E0  /* 1 / log(2) */

#endif

#define BATT_INT_RES                   0.155

#define FACTOR 100000.

static const float R1 = 6356752.;
static const float R2 = 6378137.;


float min(float val1, float val2) {
  if (val1 <= val2) return val1;
  else return val2;
}

float max(float val1, float val2) {
  if (val1 <= val2) return val2;
  else return val1;
}

double radians(double value) {
	return value * M_PI / 180.;
}

double degrees(double value) {
	return value * 180. / M_PI;
}

double sq(double value) {
	return value * value;
}

float regFen(float val_, float b1_i, float b1_f, float b2_i, float b2_f) {
  
  float x, res;
  // calcul x
  x = (val_ - b1_i) / (b1_f - b1_i);
  
  // calcul valeur: x commun
  res = x * (b2_f - b2_i) + b2_i;
  return res;
}

float regFenLim(float val_, float b1_i, float b1_f, float b2_i, float b2_f) {
  
  float x, res;
  // calcul x
  x = (val_ - b1_i) / (b1_f - b1_i);
  
  // calcul valeur: x commun
  res = x * (b2_f - b2_i) + b2_i;
  if (res < min(b2_i,b2_f)) res = min(b2_i,b2_f);
  if (res > max(b2_i,b2_f)) res = max(b2_i,b2_f);
  return res;
}

/**
 * distance_between: 24ms
 * distance_between2: 24ms
 * distance_between3: 21ms
 * distance_between4: 40ms
 *
 *
 * @param lat1 En degres
 * @param long1 En degres
 * @param lat2 En degres
 * @param long2 En degres
 * @return
 */
float distance_between2(float lat1, float long1, float lat2, float long2) {
  float delta = 3.141592 * (long1 - long2) / 180.;
  float sdlong = sinf(delta);
  float cdlong = cosf(delta);
  lat1 = 3.141592 * (lat1) / 180.;
  lat2 = 3.141592 * (lat2) / 180.;
  float slat1 = sinf(lat1);
  float clat1 = cosf(lat1);
  float slat2 = sinf(lat2);
  float clat2 = cosf(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = delta*delta;
  delta += clat2 * sdlong * clat2 * sdlong;
  delta = sqrtf(delta);
  float denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2f(delta, denom);
  return delta * 6369933.;
}

/**
 *
 * @param lat1 En degres
 * @param long1 En degres
 * @param lat2 En degres
 * @param long2 En degres
 * @return
 */
float distance_between(float lat1, float long1, float lat2, float long2) {
  float delta = 3.141592 * (long1 - long2) / 180.;
  float sdlong = arm_sin_f32(delta);
  float cdlong = arm_cos_f32(delta);
  lat1 = 3.141592 * (lat1) / 180.;
  lat2 = 3.141592 * (lat2) / 180.;
  float slat1 = arm_sin_f32(lat1);
  float clat1 = arm_cos_f32(lat1);
  float slat2 = arm_sin_f32(lat2);
  float clat2 = arm_cos_f32(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = delta*delta;
  delta += clat2 * sdlong * clat2 * sdlong;
  APP_ERROR_CHECK(arm_sqrt_f32(delta, &delta));
  float denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2f(delta, denom);
  return delta * 6369933.;
}

/**
 * Approximation petits angles sur une Terre ellipsoidale
 *
 * @param lat1 En degres
 * @param long1 En degres
 * @param lat2 En degres
 * @param long2 En degres
 * @return
 */
float distance_between3(float lat1, float long1, float lat2, float long2) {

  static float Rm = 6356752.;
  static float latRm = 0.;

  float lat1rad = M_PI * lat1 / 180.;

  float cos2lat1 = arm_cos_f32(lat1rad);
  cos2lat1 *= cos2lat1;

  if (fabsf(latRm - lat1rad) > 0.008) {
	  latRm = lat1rad;
	  APP_ERROR_CHECK(arm_sqrt_f32(R1*R1*(1-cos2lat1) + R2*R2*cos2lat1, &Rm));
  }

  // petits angles: tan = Id
  float deltalat = M_PI * (lat2 -lat1) / 180.;
  float deltalon = M_PI * (long2-long1) / 180.;

  float dhori  = deltalon * R2 * arm_cos_f32(lat1rad);
  float dverti = deltalat * Rm;

  // projection plane et pythagore
  float res = dhori*dhori + dverti*dverti;

  APP_ERROR_CHECK(arm_sqrt_f32(res, &res));

  return res;
}

/**
 * Approximation petits angles sur une Terre ellipsoidale
 *
 * @param lat1 En degres
 * @param long1 En degres
 * @param lat2 En degres
 * @param long2 En degres
 * @return
 */
float distance_between4(float lat1, float long1, float lat2, float long2) {

  static float Rm = 6356752.;
  static float latRm = 0.;

  float lat1rad = 3.141592 * lat1 / 180.;

  float cos2lat1 = powf(cosf(lat1rad), 2.);

  if (fabsf(latRm - lat1rad) > 0.008) {
	  latRm = lat1rad;
	  Rm = sqrtf(powf(R1,2.)*(1-cos2lat1) + powf(R2,2.)*cos2lat1);
  }

  // petits angles: tan = Id
  float deltalat = 3.141592 * (lat2 -lat1) / 180.;
  float deltalon = 3.141592 * (long2-long1) / 180.;

  float dhori  = deltalon * R2 * cosf(lat1rad);
  float dverti = deltalat * Rm;

  // projection plane et pythagore
  return sqrtf(dhori*dhori + dverti*dverti);
}

void calculePos (const char *nom, float *lat, float *lon) {

    static char tab[15];
    int iLat;
    int iLon;
    
    if (!nom) {
      return;
    }

    strncpy(tab, nom, 5);
    tab[5] = '\0';
    iLat = toBase10(tab);
    if (lat) {
        *lat = (float) iLat / FACTOR - 90.;
    }

    strncpy(tab, nom + 6, 2);
    strncpy(tab + 2, nom + 9, 3);
    tab[5] = '\0';
    iLon = toBase10(tab);
    if (lon) {
        *lon = (float) iLon / FACTOR - 180.;
    }

    return;
}


long unsigned int toBase10 (char *entree) {

    static char tab[15];

    if (!entree) {
        return 0;
    }
    
    if (!strstr(entree, ".")) {
        strncpy(tab, entree, 5);
        tab[5] = '\0';
    } else {
        strncpy(tab, entree, 2);
        strncpy(tab + 2, entree + 3, 3);
        tab[5] = '\0';
    }

    return strtoul(tab, NULL, 36);

}

uint32_t get_sec_jour(uint8_t hour_, uint8_t min_, uint8_t sec_)
{
  unsigned long res = 0;

  res = 3600 * hour_ + 60 * min_ + sec_;

  return res;
}


float compute2Complement(uint8_t msb, uint8_t lsb) {
	uint16_t t;
	uint16_t val;
	uint8_t tl=lsb, th=msb;
	float ret;

	if (th & 0b00100000) {
		t = th << 8;
		val = (t & 0xFF00) | (tl & 0x00FF);
		val -= 1;
		val = ~(val | 0b1110000000000000);
		//LOG_INFO("Raw 2c1: %u\r\n", val);
		ret = (float)val;
	} else {
		t = (th & 0xFF) << 8;
		val = (t & 0xFF00) | (tl & 0x00FF);
		//LOG_INFO("Raw 2c2: %u\r\n", val);
		ret = (float)-val;
	}

	return ret;
}

/**
 *
 * @param tensionValue in Volts
 * @param current
 * @return Percentage between 0 and 100
 */
float percentageBatt(float tensionValue, float current) {

    float fp_ = 0.;

    tensionValue += current * BATT_INT_RES / 1000.;

    if (tensionValue > 4.2) {
			fp_ = 100.;
    } else if (tensionValue > 3.78) {
        fp_ = 536.24 * tensionValue * tensionValue * tensionValue;
		fp_ -= 6723.8 * tensionValue * tensionValue;
        fp_ += 28186 * tensionValue - 39402;

		if (fp_ > 100.) fp_ = 100.;

    } else if (tensionValue > 3.2) {
        fp_ = powf(10, -11.4) * powf(tensionValue, 22.315);
    } else {
        fp_ = -1;
    }

    return fp_;
}


void encode_uint16 (uint8_t* dest, uint16_t input) {
	dest[0] = (uint8_t) (input & 0xFF);
	dest[1] = (uint8_t) ((input & 0xFF00) >> 8);
}

void encode_uint32 (uint8_t* dest, uint32_t input) {
	dest[0] = (uint8_t) (input & 0xFF);
	dest[1] = (uint8_t) ((input & 0xFF00) >> 8);
	dest[2] = (uint8_t) ((input & 0xFF0000) >> 16);
	dest[3] = (uint8_t) ((input & 0xFF000000) >> 24);
}

uint16_t decode_uint16 (uint8_t* dest) {
	uint16_t res = dest[0];
	res |=  dest[1] << 8;
	return res;
}

uint32_t decode_uint32 (uint8_t* dest) {
	uint32_t res = dest[0];
	res |=  dest[1] << 8;
	res |=  dest[2] << 16;
	res |=  dest[3] << 24;
	return res;
}

void const_char_to_buffer(const char *str_, uint8_t *buff_, uint16_t max_size) {

	memset(buff_, 0, max_size);

	for (uint8_t i=0; i < strlen(str_); i++) {

		if (i==max_size) {
			return;
		}

		buff_[i] = str_[i];

	}

}

/**
 * http://en.wikipedia.org/wiki/Simple_linear_regression
 *
 * @param x input arrau horizontal
 * @param y input arrau vertical
 * @param lrCoef slope=lrCoef[0] and intercept=lrCoef[1]
 * @param n length of the x and y arrays.
 */
void simpLinReg(float* x, float* y, float* lrCoef, int n) {
	// pass x and y arrays (pointers), lrCoef pointer, and n.  The lrCoef array is comprised of the slope=lrCoef[0] and intercept=lrCoef[1].  n is length of the x and y arrays.
	// http://en.wikipedia.org/wiki/Simple_linear_regression

	// initialize variables
	float xbar = 0;
	float ybar = 0;
	float xybar = 0;
	float xsqbar = 0;

	// calculations required for linear regression
	for (int i = 0; i < n; i++) {
		xbar = xbar + x[i];
		ybar = ybar + y[i];
		xybar = xybar + x[i] * y[i];
		xsqbar = xsqbar + x[i] * x[i];
	}
	xbar = xbar / n;
	ybar = ybar / n;
	xybar = xybar / n;
	xsqbar = xsqbar / n;

	// simple linear regression algorithm
	lrCoef[0] = (xybar - xbar * ybar) / (xsqbar - xbar * xbar);
	lrCoef[1] = ybar - lrCoef[0] * xbar;
}
