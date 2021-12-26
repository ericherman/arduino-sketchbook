/* max31855.c : code for the MAX31855K Thermocouple */
/* Copyright (C) 2014, 2016, 2018, 2021 Eric Herman <eric@freesa.org> */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include "max31855.h"
#include <math.h>		/* NAN */

int max31855_from_u32(struct max31855 *smax, uint32_t raw)
{
	/* this code makes use of "type punning" through a union,
	 * C (and C++) unions are a celebration of weak typing ...
	 * we have bytes in one format, then we (via a union) ask to view
	 * them as another format ... which is legal, if a bit unusual */
	union {
		uint32_t u32;
		struct max31855 max;
	} pun;

	if (!smax) {
		return -1;
	}

	pun.u32 = raw;
	*smax = pun.max;

	return max31855_error(smax);
}

int max31855_error(struct max31855 *smax)
{
	if (!smax) {
		return -1;
	}
	if (smax->err_oc) {
		return 1;
	}
	if (smax->err_scg) {
		return 2;
	}
	if (smax->err_scv) {
		return 3;
	}
	if (smax->fault) {
		return 4;
	}
	return 0;
}

/* converts from the raw 1/4 degrees */
double max31855_degrees_c(struct max31855 *smax)
{
	return smax ? ((smax->quarter_degrees * 1.0) / 4.0) : NAN;
}

/* converts from the rawl 1/16th degrees */
double max31855_internal_degrees_c(struct max31855 *smax)
{
	return smax ? ((smax->internal_sixteenths * 1.0) / 16.0) : NAN;
}

#if MAX32855_EXTERNAL_LOG_DEFS
/* the max32855.h declares the following to exist:
void log_s(void *log, const char *s);
void log_u(void *log, unsigned int u);
void log_i(void *log, int i);
void log_f(void *log, double f);
void log_eol(void *log);
*/
#else /* MAX32855_EXTERNAL_LOG_DEFS */
#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
static void log_s(void *log, const char *s)
{
	(void)log;
	Serial.print(s);
}

static void log_u(void *log, unsigned int u)
{
	(void)log;
	Serial.print(u);
}

static void log_i(void *log, int i)
{
	(void)log;
	Serial.print(i);
}

static void log_f(void *log, double f)
{
	(void)log;
	Serial.print(f);
}

static void log_eol(void *log)
{
	(void)log;
	Serial.println();
}
#else /* ARDUINO */
#ifdef __STDC_HOSTED__
#include <stdio.h>
#ifndef STDLOG
#define STDLOG(log) (log ? ((FILE *)log) : stdout)
#endif /* STDLOG */
static void log_s(void *log, const char *s)
{
	fprintf(STDLOG(log), "%s", s);
}

static void log_u(void *log, unsigned int u)
{
	fprintf(STDLOG(log), "%u", u);
}

static void log_i(void *log, int i)
{
	fprintf(STDLOG(log), "%d", i);
}

static void log_f(void *log, double f)
{
	fprintf(STDLOG(log), "%g", f);
}

static void log_eol(void *log)
{
	fprintf(STDLOG(log), "\n");
}
#endif /* __STDC_HOSTED__ */
#endif /* ARDUINO */
#endif /* MAX32855_EXTERNAL_LOG_DEFS */

void max31855_log(void *log, struct max31855 *smax)
{
	if (!smax) {
		log_s(log, NULL);
		log_eol(log);
		return;
	}

	log_s(log, "{");
	log_eol(log);

	log_s(log, "\tquarter_degrees: ");
	log_i(log, smax->quarter_degrees);
	log_s(log, " (");
	log_f(log, max31855_degrees_c(smax));
	log_s(log, ")");
	log_eol(log);

	log_s(log, "\treserved (");
	log_u(log, smax->reserved17);
	log_s(log, ")");
	log_eol(log);

	log_s(log, "\tfault: ");
	log_u(log, smax->fault);
	log_eol(log);

	log_s(log, "\tinternal_sixteenths: ");
	log_i(log, smax->internal_sixteenths);
	log_s(log, " (");
	log_f(log, max31855_internal_degrees_c(smax));
	log_s(log, ")");
	log_eol(log);

	log_s(log, "\treserved (");
	log_u(log, smax->reserved3);
	log_s(log, ")");
	log_eol(log);

	log_s(log, "\terr_scv: ");
	log_u(log, smax->err_scv);
	if (smax->err_scv) {
		log_s(log, " Shorted to Vcc!");
	}
	log_eol(log);

	log_s(log, "\trr_scg: ");
	log_u(log, smax->err_scg);
	if (smax->err_scg) {
		log_s(log, " Shorted to GND!");
	}
	log_eol(log);

	log_s(log, "\terr_oc: ");
	log_u(log, smax->err_oc);
	if (smax->err_oc) {
		log_s(log, " Open Circuit!");
	}
	log_eol(log);

	log_s(log, "}");
	log_eol(log);
}

double max31855_celsius_to_fahrenheit(double celsius)
{
	return (celsius * 9.0 / 5.0) + 32.0;
}

double max31855_celsius_to_kelvin(double celsius)
{
	return celsius + 273.15;
}

double max31855_celsius_to_rankine(double celsius)
{
	return (celsius + 273.15) * 9.0 / 5.0;
}
