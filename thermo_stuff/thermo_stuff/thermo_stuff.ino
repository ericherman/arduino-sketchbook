/* thermo_stuff.ino : Arduino code for the MAX31855K Thermocouple */
/* Copyright (C) 2014, 2016, 2018, 2021 Eric Herman <eric@freesa.org> */
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* includes: https://github.com/ericherman/simple_stats */

#include <Arduino.h>
#include <SPI.h>
#include <limits.h>

#include "simple_stats.h"
#include "max31855.h"

const uint32_t seconds_per_reading = 1;
const uint32_t too_warm_temp_c = 90;
const uint32_t too_cold_temp_c = 60;

/* const uint32_t microseconds_per_second = 1000 * 1000; */
const uint32_t milliseconds_per_second = 1000;

const uint32_t max_milliseconds_off = 2 * 60 * milliseconds_per_second;
const uint32_t max_milliseconds_on = 50 * milliseconds_per_second;

/* If we're reading below too_cold_temp_c, equal time off and on */
const uint32_t cold_max_milliseconds_on = max_milliseconds_off;

const unsigned long milliseconds_per_reading =
    (seconds_per_reading * milliseconds_per_second);
const unsigned long microseconds_between_samples = 500;

/* INPUT PINS */

/* OUTPUT PINS */
const uint8_t heater_pin = 5;
const uint8_t blink_pin = 6;
const uint8_t too_cold_pin = 7;
const uint8_t too_warm_pin = 8;
const uint8_t error_pin = 9;

/* INTERNAL PINS */
const uint8_t thermo_couple_chip_select_pin = 2;

/* globals */
uint32_t loop_count = 0;
unsigned long target_millis = 0;
int heater_status = LOW;

/* POSIX and Windows define CHAR_BIT to be 8, but embedded platforms vary */
#ifndef CHAR_BIT
#error "CHAR_BIT not defined"
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

void setup(void)
{
	pinMode(thermo_couple_chip_select_pin, OUTPUT);

	pinMode(heater_pin, OUTPUT);
	pinMode(blink_pin, OUTPUT);
	pinMode(too_cold_pin, OUTPUT);
	pinMode(too_warm_pin, OUTPUT);
	pinMode(error_pin, OUTPUT);

	digitalWrite(thermo_couple_chip_select_pin, HIGH);
	digitalWrite(heater_pin, heater_status);
	digitalWrite(blink_pin, LOW);
	digitalWrite(too_cold_pin, LOW);
	digitalWrite(too_warm_pin, LOW);
	digitalWrite(error_pin, LOW);

	SPI.begin();

	/* Let IC stabilize to avoid first reading as garbage */
	delay(50);

	const int tty_baud = 9600;
	Serial.begin(tty_baud);
	Serial.println();
	Serial.println("Start");
	Serial.println();

	Serial.println("=================================================");
	Serial.println("Note, struct max31855 is a special type of uint32");
	Serial.print("        sizeof(uint32_t) == ");
	Serial.println(sizeof(uint32_t));
	Serial.print(" sizeof(struct max31855) == ");
	Serial.println(sizeof(struct max31855));
	Serial.println("=================================================");
	Serial.println();

	Serial.print("Seconds per Reading: ");
	Serial.println((double)milliseconds_per_reading /
		       (double)milliseconds_per_second);
	Serial.print("microseconds delay between samples: ");
	Serial.println(microseconds_between_samples);
	Serial.println("\n");

	target_millis = millis();
}

void loop(void)
{
	unsigned long mnow = millis();

	digitalWrite(error_pin, LOW);

	++loop_count;
	digitalWrite(blink_pin, ((loop_count % 2) == 0) ? LOW : HIGH);

	simple_stats ss_temperature;
	simple_stats_init(&ss_temperature);

	uint32_t saved_error_raw = 0;

	unsigned int errors = 0;
	for (size_t i = 0; millis() < (mnow + milliseconds_per_reading); ++i) {
		uint32_t raw = 0;
		raw = spi_read_big_endian_uint32(thermo_couple_chip_select_pin);
		struct max31855 smax;
		if (max31855_from_u32(&smax, raw)) {
			++errors;
			saved_error_raw = raw;
		} else {
			double d = max31855_degrees_c(&smax);
			simple_stats_append_val(&ss_temperature, d);
		}
		digitalWrite(error_pin, max31855_error(&smax) ? HIGH : LOW);
		delayMicroseconds(microseconds_between_samples);
	}

	Serial.print(mnow / milliseconds_per_second);
	Serial.print(", heater status: ");
	Serial.print(heater_status == HIGH ? "ON " : "off");
	Serial.print(" until ");
	Serial.print(1 + (target_millis / milliseconds_per_second));
	Serial.print(", ");
	int too_cold = 0;
	int too_warm = 0;
	if (ss_temperature.cnt) {
		double avg_temp = simple_stats_average(&ss_temperature);

		Serial.print("temp: ");
		Serial.print(avg_temp);
		Serial.print(" +/-");
		int bessel_correct = 1;
		Serial.print(simple_stats_std_dev
			     (&ss_temperature, bessel_correct));

		Serial.print(" (min: ");
		Serial.print(ss_temperature.min);
		Serial.print(", max: ");
		Serial.print(ss_temperature.max);
		Serial.print(", cnt: ");
		Serial.print(ss_temperature.cnt);
		Serial.print(", err: ");
		Serial.print(errors);
		Serial.print(")");

		too_cold = (avg_temp < too_cold_temp_c) ? 1 : 0;
		digitalWrite(too_cold_pin, too_cold ? HIGH : LOW);

		too_warm = (avg_temp > too_warm_temp_c) ? 1 : 0;
		digitalWrite(too_warm_pin, too_warm ? HIGH : LOW);

		if (too_warm) {
			Serial.print(" TOO HOT!");
		}

		Serial.println();
	}

	if (errors) {
		digitalWrite(error_pin, HIGH);

		Serial.print("Errors: ");
		Serial.print(errors);
		Serial.print(" last error raw value: ");
		Serial.print(saved_error_raw);
		Serial.print(" == ");
		struct max31855 error_max;
		max31855_from_u32(&error_max, saved_error_raw);
		max31855_log(NULL, &error_max);
		Serial.println();

		Serial.print("Sleeping ...");
		if (ss_temperature.cnt > errors) {
			/* got some values, but some errors, add small delay */
			/* but do not change the target sample interval */
			delay(milliseconds_per_reading / 8);
		} else {
			Serial.println(" and adjusting target to change ...");
			/* things are broken add large delay */
			delay(milliseconds_per_reading * 4);
			/* and postpone the next sample evaluation */
			target_millis += (milliseconds_per_reading * 4);
		}
		Serial.println("waking.");

		digitalWrite(error_pin, LOW);
	}

	if (mnow > target_millis) {
		if (too_warm || heater_status == HIGH) {
			heater_status = LOW;
			digitalWrite(heater_pin, heater_status);
			target_millis += max_milliseconds_off;
		} else {
			heater_status = HIGH;
			digitalWrite(heater_pin, HIGH);
			if (too_cold) {
				target_millis += cold_max_milliseconds_on;
			} else {
				target_millis += max_milliseconds_on;
			}
		}
	}
}
