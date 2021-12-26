/* thermo_stuff.ino : Arduino code for the MAX31855K Thermocouple */
/* Copyright (C) 2014, 2016, 2018, 2021 Eric Herman <eric@freesa.org> */
/* License: GPLv3 or later */
/* includes: https://github.com/ericherman/simple_stats */

#include <Arduino.h>
#include <SPI.h>
#include <limits.h>

#include "simple_stats.h"
#include "max31855.h"

#define Seconds_per_reading 1
#define Way_over_temp_c	90
#define Too_cold_temp_c	60

#define Microseconds_per_second (1000UL * 1000UL)
#define Milliseconds_per_second (1000UL)

const uint32_t max_milliseconds_off = 2 * 60 * Milliseconds_per_second;
const uint32_t max_milliseconds_on = 50 * Milliseconds_per_second;

/* If we're reading below Too_cold_temp_c, equal time off and on */
const uint32_t cold_max_milliseconds_on = max_milliseconds_off;

const unsigned long milliseconds_per_reading =
    (Seconds_per_reading * Milliseconds_per_second);
const unsigned long microseconds_between_samples = 500;

#define TTY_BAUD 9600

/* INTERNAL PINS */
#define Thermo_couple_chip_select_pin 2

/* INPUT PINS */

/* OUTPUT PINS */
#define HEATER_PIN 5
#define BLINK_PIN 6
#define TOO_COLD_STATUS_PIN 7
#define WAY_OVER_STATUS_PIN 8
#define ERROR_STATUS_PIN 9

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
	pinMode(Thermo_couple_chip_select_pin, OUTPUT);

	pinMode(HEATER_PIN, OUTPUT);
	pinMode(BLINK_PIN, OUTPUT);
	pinMode(TOO_COLD_STATUS_PIN, OUTPUT);
	pinMode(WAY_OVER_STATUS_PIN, OUTPUT);
	pinMode(ERROR_STATUS_PIN, OUTPUT);

	digitalWrite(Thermo_couple_chip_select_pin, HIGH);
	digitalWrite(HEATER_PIN, heater_status);
	digitalWrite(BLINK_PIN, LOW);
	digitalWrite(TOO_COLD_STATUS_PIN, LOW);
	digitalWrite(WAY_OVER_STATUS_PIN, LOW);
	digitalWrite(ERROR_STATUS_PIN, LOW);

	SPI.begin();

	/* Let IC stabilize to avoid first reading as garbage */
	delay(50);

	Serial.begin(TTY_BAUD);
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
		       (double)Milliseconds_per_second);
	Serial.print("microseconds delay between samples: ");
	Serial.println(microseconds_between_samples);
	Serial.println("\n");

	target_millis = millis();
}

void loop(void)
{
	unsigned long mnow = millis();

	digitalWrite(ERROR_STATUS_PIN, LOW);

	++loop_count;
	digitalWrite(BLINK_PIN, ((loop_count % 2) == 0) ? LOW : HIGH);

	simple_stats ss_temperature;
	simple_stats_init(&ss_temperature);

	uint32_t saved_error_raw = 0;

	unsigned int errors = 0;
	for (size_t i = 0; millis() < (mnow + milliseconds_per_reading); ++i) {
		uint32_t raw = 0;
		raw = spi_read_big_endian_uint32(Thermo_couple_chip_select_pin);
		struct max31855 smax;
		if (max31855_from_u32(&smax, raw)) {
			++errors;
			saved_error_raw = raw;
		} else {
			double d = max31855_degrees_c(&smax);
			simple_stats_append_val(&ss_temperature, d);
		}
		digitalWrite(ERROR_STATUS_PIN,
			     max31855_error(&smax) ? HIGH : LOW);
		delayMicroseconds(microseconds_between_samples);
	}

	Serial.print(mnow / Milliseconds_per_second);
	Serial.print(", heater status: ");
	Serial.print(heater_status == HIGH ? "ON " : "off");
	Serial.print(" until ");
	Serial.print(1 + (target_millis / Milliseconds_per_second));
	Serial.print(", ");
	int too_cold = 0;
	int way_over = 0;
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

		too_cold = (avg_temp < Too_cold_temp_c) ? 1 : 0;
		digitalWrite(TOO_COLD_STATUS_PIN, too_cold ? HIGH : LOW);

		way_over = (avg_temp > Way_over_temp_c) ? 1 : 0;
		digitalWrite(WAY_OVER_STATUS_PIN, way_over ? HIGH : LOW);

		if (way_over) {
			Serial.print(" TOO HOT!");
		}

		Serial.println();
	}

	if (errors) {
		digitalWrite(ERROR_STATUS_PIN, HIGH);

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

		digitalWrite(ERROR_STATUS_PIN, LOW);
	}

	if (mnow > target_millis) {
		if (way_over || heater_status == HIGH) {
			heater_status = LOW;
			digitalWrite(HEATER_PIN, heater_status);
			target_millis += max_milliseconds_off;
		} else {
			heater_status = HIGH;
			digitalWrite(HEATER_PIN, HIGH);
			if (too_cold) {
				target_millis += cold_max_milliseconds_on;
			} else {
				target_millis += max_milliseconds_on;
			}
		}
	}
}
