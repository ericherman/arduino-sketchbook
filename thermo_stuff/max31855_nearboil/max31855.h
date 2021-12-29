/* max31855.h : code for the MAX31855K Thermocouple */
/* Copyright (C) 2014, 2016, 2018, 2021 Eric Herman <eric@freesa.org> */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifndef MAX31855_H
#define MAX31855_H 1

#ifdef __cplusplus
#define Max31855_begin_C_functions extern "C" {
#define Max31855_end_C_functions }
#else
#define Max31855_begin_C_functions
#define Max31855_end_C_functions
#endif

Max31855_begin_C_functions
#undef Max31855_begin_C_functions
#include <stdint.h>
/* we are unlikely to need these obvious conversion functions */
double max31855_celsius_to_fahrenheit(double celsius);
double max31855_celsius_to_kelvin(double celsius);
double max31855_celsius_to_rankine(double celsius);

/* if you happen to have a bigendian chip, make a reversed "be" version
 * e.g.: #if ( __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ ), but until we have
 * a BE machine to test with, assume Endian Little */
#define max31855 max31855_le

/*
 * See also: sparkfun's demo at
 *  https://github.com/sparkfun/MAX31855K_Thermocouple_Digitizer
 */

/*
 * the source data is Network Byte Order (bigendian)
 * our host is very likely little endian, thus we need
 * lay out fields in our struct in the opposite order
 * that they are described in the data sheet
 *
 * this struct uses the C (and C++) syntax for specifying structs with
 * members as bit-fields ... this structure is exactly 32 bits
 */
struct max31855_le {
	unsigned int err_oc:1;	/* D0: open circuit */
	unsigned int err_scg:1;	/* D1: shorted to GND */
	unsigned int err_scv:1;	/* D2: shorted to Vcc */
	unsigned int reserved3:1;	/* D3: reserved */
	int internal_sixteenths:12;	/* D4-15: INTERNAL TEMPERATURE */
	unsigned int fault:1;	/* D16: FAULT */
	unsigned int reserved17:1;	/* D17: reserved */
	int quarter_degrees:14;	/* D18-D31: THERMOCOUPLE TEMPERATURE DATA */
};

/* Takes the "raw" 32bit value (probably read from the SPI),
 * converts it to a max31855
 * returns 0 on success */
int max31855_from_u32(struct max31855 *smax, uint32_t raw);

/* returns 0 on success, non-zero on error */
int max31855_error(struct max31855 *smax);

double max31855_degrees_c(struct max31855 *smax);
double max31855_internal_degrees_c(struct max31855 *smax);

/*
 * Unless max31855.c was compiled with MAX32855_EXTERNAL_LOG_DEFS:
 *
 * For typical non-embedded __STDC_HOSTED__ systems, *log must be NULL or a
 * a FILE pointer. If NULL, stdout is used.
 *
 * For Arduino, *log is ignored, and Serial.print(x) is used.
 */
void max31855_log(void *log, struct max31855 *smax);

/* if using another embedded platform, be sure to create the based output
 * functions */
#if MAX32855_EXTERNAL_LOG_DEFS
void log_s(void *log, const char *s);
void log_u(void *log, unsigned int u);
void log_i(void *log, int i);
void log_f(void *log, double f);
void log_eol(void *log);
#endif

Max31855_end_C_functions
#undef Max31855_end_C_functions
#endif /* MAX31855_H */
