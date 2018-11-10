/* thermostuf2.ino : Arduino code for the MAX31855K Thermocouple
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
/* includes: https://github.com/ericherman/simple_stats */

#include <Arduino.h>
#include <SPI.h>
#include <float.h>
#include <math.h>

#define TTY_BAUD 9600
#define CHIP_SELECT_PIN 10
#define LED_PIN 8

#define Microseconds_per_second (1000UL * 1000UL)
const unsigned long microseconds_per_reading = 2 * Microseconds_per_second;
const unsigned long microseconds_between_samples = 500;

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

/* POSIX and Windows define CHAR_BIT to be 8, but embedded platforms vary */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#if (CHAR_BIT != 8)
#error "CHAR_BUT != 8"
#endif
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

#define ErrorPrint(val) Serial.print(val)
#define ErrorPrintln(val) Serial.println(val)
void debug_max31855(uint32_t raw)
{
	struct max31855_s smax;

	max31855_from_u32(&smax, raw);

	ErrorPrint("max31855_from_u32(");
	ErrorPrint(raw);
	ErrorPrintln(") {");

	ErrorPrint("\tquarter_degrees: ");
	ErrorPrint(smax.quarter_degrees);
	ErrorPrint(" (");
	ErrorPrint(((smax.quarter_degrees * 1.0) / 4.0));
	ErrorPrintln(")");

	ErrorPrint("\treserved (");
	ErrorPrint(smax.reserved17);
	ErrorPrintln(")");

	ErrorPrint("\tfault: ");
	ErrorPrint(smax.fault);
	ErrorPrintln();

	ErrorPrint("\tinternal_sixteenths: ");
	ErrorPrint(smax.internal_sixteenths);
	ErrorPrint(" (");
	ErrorPrint(((smax.internal_sixteenths * 1.0) / 16.0));
	ErrorPrintln(")");

	ErrorPrint("\treserved (");
	ErrorPrint(smax.reserved3);
	ErrorPrintln(")");

	ErrorPrint("\terr_scv: ");
	ErrorPrint(smax.err_scv);
	ErrorPrintln();

	ErrorPrint("\trr_scg: ");
	ErrorPrint(smax.err_scg);
	ErrorPrintln();

	ErrorPrint("\terr_oc: ");
	ErrorPrint(smax.err_oc);
	ErrorPrintln();

	ErrorPrintln("}");
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

typedef struct simple_stats_s {
	unsigned int cnt;
	double min;
	double max;
	double sum;
	double sum_of_squares;
} simple_stats;

static void simple_stats_init(simple_stats *stats);
static void simple_stats_append_val(simple_stats *stats, double val);
static double simple_stats_average(simple_stats *stats);
static double simple_stats_variance(simple_stats *stats, int bessel_correct);
static double simple_stats_std_dev(simple_stats *stats, int bessel_correct);

static void simple_stats_init(simple_stats *stats)
{
	stats->cnt = 0;
	stats->min = DBL_MAX;
	stats->max = -DBL_MAX;
	stats->sum = 0.0;
	stats->sum_of_squares = 0.0;
}

static void simple_stats_append_val(simple_stats *stats, double val)
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

static double simple_stats_average(simple_stats *stats)
{
	return stats->sum / stats->cnt;
}

static double simple_stats_variance(simple_stats *stats, int bessel_correct)
{
	double avg_sum_squared, avg_diff_sum_sq, variance;
	size_t bassel_cnt;

	/*   avoid division by zero */
	if (stats->cnt == 0) {
		return NAN;
	}
	if (stats->cnt == 1) {
		return 0.0;
	}

	/* https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance */

	/*  Because SumSq and (Sum * Sum)/n can be very similar
	 *  numbers, cancellation can lead to the precision of the result to
	 *  be much less than the inherent precision of the floating-point
	 *  arithmetic used to perform the computation. Thus this algorithm
	 *  should not be used in practice. */

	bassel_cnt = bessel_correct ? (stats->cnt - 1) : stats->cnt;
	avg_sum_squared = (stats->sum * stats->sum) / stats->cnt;
	avg_diff_sum_sq = stats->sum_of_squares - avg_sum_squared;
	variance = avg_diff_sum_sq / bassel_cnt;
	return fabs(variance);
}

static double simple_stats_std_dev(simple_stats *stats, int bessel_correct)
{
	return sqrt(simple_stats_variance(stats, bessel_correct));
}

void setup()
{
	pinMode(LED_PIN, OUTPUT);

	pinMode(CHIP_SELECT_PIN, OUTPUT);
	digitalWrite(CHIP_SELECT_PIN, HIGH);

	SPI.begin();

	/* Let IC stabilize or first readings will be garbage */
	delay(50);

	Serial.begin(TTY_BAUD);
	Serial.println("\nStart\n");
	Serial.print("Seconds per Reading: ");
	Serial.println((double)microseconds_per_reading /
		     (double)Microseconds_per_second);
	Serial.print("microseconds delay between samples: ");
	Serial.println(microseconds_between_samples);
	Serial.println("\n");
}

uint32_t loop_count = 0;
void loop()
{
	uint32_t raw;
	struct max31855_s smax;
	simple_stats ss_temperature, ss_internal_temp;
	int bessel_correct = 1;
	int error, enough;
	size_t i;

	++loop_count;
	digitalWrite(LED_PIN, ((loop_count % 2) == 0) ? LOW : HIGH);

	simple_stats_init(&ss_temperature);
	simple_stats_init(&ss_internal_temp);

	unsigned long start_micros = micros();
	for (i = 0, enough = 0, error = 0; !error && !enough; ++i) {
		raw = spi_read_big_endian_uint32(CHIP_SELECT_PIN);
		max31855_from_u32(&smax, raw);
		if (smax.fault) {
			error = 1;
		} else {
			double d = max31855_degrees(&smax);
			simple_stats_append_val(&ss_temperature, d);
			d = max31855_internal(&smax);
			simple_stats_append_val(&ss_internal_temp, d);
			delayMicroseconds(microseconds_between_samples);
		}
		unsigned long now_micros = micros();
		if (now_micros < start_micros) {
			enough = 1;
		} else {
			unsigned long elapsed_micros =
			    now_micros - start_micros;
			if (elapsed_micros >= microseconds_per_reading) {
				enough = 1;
			}
		}
	}

	Serial.print(loop_count);
	Serial.print(", ");
	Serial.print(simple_stats_average(&ss_temperature));
	Serial.print(" +/-");
	Serial.print(simple_stats_std_dev(&ss_temperature, bessel_correct));
	Serial.print(" (min: ");
	Serial.print(ss_temperature.min);
	Serial.print(", max: ");
	Serial.print(ss_temperature.max);
	Serial.print(", cnt: ");
	Serial.print(ss_temperature.cnt);
	Serial.print("), internal: ");
	Serial.print(simple_stats_average(&ss_internal_temp));
	Serial.print(" +/-");
	Serial.print(simple_stats_std_dev(&ss_internal_temp, bessel_correct));
	Serial.println();

	if (error) {
		ErrorPrint("[");
		ErrorPrint(i);
		ErrorPrint("]");
		debug_max31855(raw);
		delay(1000);
		ErrorPrintln();
	}
}
