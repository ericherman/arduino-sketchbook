/*
   Copyright (C) 2014, 2016, 2018 Eric Herman <eric@freesa.org>

   This work is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This work is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License and the GNU General Public License for
   more details.

   You should have received a copy of the GNU Lesser General Public
   License (COPYING) and the GNU General Public License (COPYING.GPL3).
   If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <SPI.h>
#include <limits.h>		/* UINT_MAX */
#include <float.h>		/* DBL_MAX */
#include <math.h>		/* sqrt */

#define TTY_BAUD 9600
#define CHIP_SELECT_PIN 10
#define LED_PIN 8
#define DELAY_MICROS_BETWEEN_SAMPLES (500U)

#define ENABLE_DEBUG 0

#define Eprint(val) Serial.print(val)
#define Eprintln(val) Serial.println(val)

typedef struct simple_stats_s {
	unsigned int cnt;
	double min;
	double max;
	double sum;
	double sum_of_squares;
} simple_stats;

void simple_stats_init(simple_stats * stats)
{
	stats->cnt = 0;
	stats->min = DBL_MAX;
	stats->max = -DBL_MAX;
	stats->sum = 0.0;
	stats->sum_of_squares = 0.0;
}

void simple_stats_append_val(simple_stats * stats, double val)
{
	stats->cnt++;
	if (stats->min > val) {
		stats->min = val;
	}
	if (stats->max < val) {
		stats->max = val;
	}
	stats->sum += val;
	stats->sum_of_squares += (val * val);
}

double simple_stats_average(simple_stats * stats)
{
	return stats->sum / stats->cnt;
}

double simple_stats_variance(simple_stats * stats)
{
	return stats->sum_of_squares / stats->cnt;
}

double simple_stats_std_dev(simple_stats * stats)
{
	return sqrt(simple_stats_variance(stats));
}

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/*
 * See also: sparkfun's demo at
 *  https://github.com/sparkfun/MAX31855K_Thermocouple_Digitizer
 */

/* the source data is Network Byte Order (bigendian)
 * our host is very likely little endian, thus we need
 * lay out fields in our struct in the opposite order
 * that they are described in the data sheet
 *
 * this struct uses the C (and C++) syntax for specifying structs with
 * members as bit-fields ... this structure is exactly 32 bits
 */
struct max31855_le_s {
	unsigned int err_oc:1;	/* D0: open circuit */
	unsigned int err_scg:1;	/* D1: shorted to GND */
	unsigned int err_scv:1;	/* D2: shorted to Vcc */
	unsigned int reserved3:1;	/* D3: reserved */
	int internal_sixteenths:12;	/* D4-15: INTERNAL TEMPERATURE */
	unsigned int fault:1;	/* D16: FAULT */
	unsigned int reserved17:1;	/* D17: reserved */
	int quarter_degrees:14;	/* D18-D31: THERMOCOUPLE TEMPERATURE DATA */
};
/* if you happen to have a bigendian chip, make a reversed "be" version */
#define max31855_s max31855_le_s

/* this code makes use of "type punning" through a union,
 * C (and C++) unions are a celebration of weak typing ...
 * we have bytes in one format, then we (via a union) ask to view
 * them as another format ... which is legal, if a bit unusual */
static void max31855_from_u32(struct max31855_s *max31855, uint32_t raw)
{
	union {
		uint32_t u32;
		struct max31855_s max;
	} pun;

	if (!max31855) {
		return;
	}

	pun.u32 = raw;
	*max31855 = pun.max;
}

/* converts from the raw 1/4 degrees to "real" units */
static float max31855_degrees(struct max31855_s *smax)
{
	return smax ? ((smax->quarter_degrees * 1.0) / 4.0) : NAN;
}

/* converts from the rawl 1/16th degrees to "real" units */
static float max31855_internal(struct max31855_s *smax)
{
	return smax ? ((smax->internal_sixteenths * 1.0) / 16.0) : NAN;
}

void debug_max31855(uint32_t raw)
{
	struct max31855_s smax;

	max31855_from_u32(&smax, raw);

	Eprint("max31855_from_u32(");
	Eprint(raw);
	Eprintln(") {");

	Eprint("\tquarter_degrees: ");
	Eprint(smax.quarter_degrees);
	Eprint(" (");
	Eprint(((smax.quarter_degrees * 1.0) / 4.0));
	Eprintln(")");

	Eprint("\treserved (");
	Eprint(smax.reserved17);
	Eprintln(")");

	Eprint("\tfault: ");
	Eprint(smax.fault);
	Eprintln();

	Eprint("\tinternal_sixteenths: ");
	Eprint(smax.internal_sixteenths);
	Eprint(" (");
	Eprint(((smax.internal_sixteenths * 1.0) / 16.0));
	Eprintln(")");

	Eprint("\treserved (");
	Eprint(smax.reserved3);
	Eprintln(")");

	Eprint("\terr_scv: ");
	Eprint(smax.err_scv);
	Eprintln();

	Eprint("\trr_scg: ");
	Eprint(smax.err_scg);
	Eprintln();

	Eprint("\terr_oc: ");
	Eprint(smax.err_oc);
	Eprintln();

	Eprintln("}");
}

uint32_t spi_read_big_endian_uint32(uint8_t cs)
{
	uint8_t bytes[4];
	uint32_t u32;

	digitalWrite(cs, LOW);

	SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

	bytes[3] = SPI.transfer(0x00);
	bytes[2] = SPI.transfer(0x00);
	bytes[1] = SPI.transfer(0x00);
	bytes[0] = SPI.transfer(0x00);

	SPI.endTransaction();

	digitalWrite(cs, HIGH);

	u32 = ((uint32_t)bytes[3]) << (3 * CHAR_BIT)
	    | ((uint32_t)bytes[2]) << (2 * CHAR_BIT)
	    | ((uint32_t)bytes[1]) << (1 * CHAR_BIT)
	    | ((uint32_t)bytes[0]) << (0 * CHAR_BIT);

	return u32;
}

/* if you need non-celsius, use these */
/*
static float celsius_to_fahrenheit(float celsius)
{
	return (celsius * 9.0 / 5.0) + 32.0;
}

static float celsius_to_kelvin(float celsius)
{
	return celsius + 273.15;
}

static float celsius_to_rankine(float celsius)
{
	return (celsius + 273.15) * 9.0 / 5.0;
}
*/

void setup()
{
	pinMode(LED_PIN, OUTPUT);

	pinMode(CHIP_SELECT_PIN, OUTPUT);
	digitalWrite(CHIP_SELECT_PIN, HIGH);

	SPI.begin();

	/* Let IC stabilize or first readings will be garbage */
	delay(50);

	Serial.begin(TTY_BAUD);
	Eprintln("\nStart\n");
}

uint32_t loop_count = 0;
void loop()
{
	unsigned int sample_count, error_count;
	unsigned int then, now;
	float ext_temp, int_temp;
	simple_stats external_temp_stats;
	simple_stats internal_temp_stats;
	uint32_t raw;
	struct max31855_s smax;

	simple_stats_init(&external_temp_stats);
	simple_stats_init(&internal_temp_stats);

	++loop_count;
	digitalWrite(LED_PIN, ((loop_count % 2) == 0) ? LOW : HIGH);

	sample_count = 0;
	error_count = 0;
	then = millis() / 1000;
	do {
		raw = spi_read_big_endian_uint32(CHIP_SELECT_PIN);

		max31855_from_u32(&smax, raw);

		if (ENABLE_DEBUG && smax.fault) {
			debug_max31855(raw);
			delay(2000);
			return;
		}
		if (smax.fault) {
			++error_count;
		} else {
			++sample_count;
			ext_temp = max31855_degrees(&smax);
			simple_stats_append_val(&external_temp_stats, ext_temp);
			int_temp = max31855_internal(&smax);
			simple_stats_append_val(&internal_temp_stats, int_temp);
		}
		delayMicroseconds(DELAY_MICROS_BETWEEN_SAMPLES);
		now = millis() / 1000;
	} while (now == then);

	Serial.print(loop_count);
	Serial.print(" temp: ");
	Serial.print(simple_stats_average(&external_temp_stats));
	Serial.print(" (min: ");
	Serial.print(external_temp_stats.min);
	Serial.print(" max: ");
	Serial.print(external_temp_stats.max);
	Serial.print(" cnt: ");
	Serial.print(external_temp_stats.cnt);
	if (error_count) {
		Serial.print(" err: ");
		Serial.print(error_count);
	}
	Serial.print(")");
	Serial.print(" (ambient: ");
	Serial.print(simple_stats_average(&internal_temp_stats));
	Serial.print(")");
	Serial.println();
}
