/* float-transmission.c: simple float persistance to byte device */
/* Copyright 2020 (C) Eric Herman */
/* License: WTFPL – Do What the Fuck You Want to Public License */
/* http://www.wtfpl.net */

#ifdef ARDUINO

#include <Arduino.h>
#include <HardwareSerial.h>

#endif

#include <stddef.h>

/* function prototypes for C compat */
void print_s(const char *str);
void print_f(float f);
void print_z(size_t z);
void print_x(unsigned char x);
void print_eol(void);
int cmp_floats_nan_safe(float a, float b);

/* there are byte order issues to consider if going between machines of
 * different endian-ness, but not needed in our simple persistance case */

/* this is intentially verbose to make a clear example
 * do it in fewer lines in your own firmware */
void bytes_from_float(unsigned char *bytes, float f)
{
	float *fptr = NULL;
	unsigned char *fbytes = NULL;

	fptr = &f;
	fbytes = (unsigned char *)fptr;

	for (size_t i = 0; i < sizeof(float); ++i) {
		bytes[i] = fbytes[i];
	}
}

float float_from_bytes(unsigned char *bytes)
{
	float *fbytes = NULL;
	float f = 0.0;

	fbytes = (float *)bytes;
	f = *fbytes;

	return f;
}

int check_round_trip(float original)
{
	unsigned char float_byte_buffer[sizeof(float)];
	float round_trip = 0.0;

	print_s("original      == ");
	print_f(original);
	print_eol();

	bytes_from_float(float_byte_buffer, original);

	print_s("bytes         == 0x");
	for (size_t i = 0; i < sizeof(float); ++i) {
		print_x(float_byte_buffer[i]);
	}
	print_eol();

	round_trip = float_from_bytes(float_byte_buffer);

	print_s("round_trip    == ");
	print_f(round_trip);
	print_eol();

	print_eol();

	int error = cmp_floats_nan_safe(original, round_trip);

	print_s("do they match?   ");
	print_s(error ? "no" : "yes");
	print_eol();

	return error;
}

int cmp_floats_nan_safe(float a, float b)
{
	if (a == b) {
		return 0;
	}

	/* NaN is _never_ equal to itself */
	if (a != a || b != b) {
		return (a != a && b != b) ? 0 : (a != a) ? -1 : 1;
	}

	return (a < b) ? -1 : 1;
}

#ifdef ARDUINO

void setup(void)
{
	Serial.begin(9600);
	delay(50);

	Serial.println();
	Serial.println();

	Serial.println("Begin");

	Serial.println();

	Serial.print("sizeof(float) == ");
	Serial.println(sizeof(float));

	Serial.println();
}

unsigned loop_count = 0;
void loop(void)
{
	Serial.println();

	Serial.print("Begin loop #");
	Serial.println(++loop_count);
	Serial.println();

	/* try some different numbers */
	float original = (loop_count % 17)
	    + (2.0 / (2.0 + loop_count))
	    + (1.0 / (1.0 + millis()));

	int error = check_round_trip(original);

	Serial.println();

	Serial.print("Loop ");
	Serial.print(loop_count);
	Serial.print(" result for ");
	Serial.print(original);
	Serial.println(error ? ": FAIL!" : ": SUCCESS!");

	uint16_t delay_ms = 5 * 1000;
	unsigned dots = 50;
	for (size_t i = 0; i < dots; ++i) {
		Serial.print(".");
		delay(delay_ms / dots);
	}
	Serial.println();
}

void print_s(const char *str)
{
	Serial.print(str);
}

void print_f(float f)
{
	Serial.print(f, 5);
}

void print_z(size_t z)
{
	Serial.print(z);
}

static char _nibble_to_hex(unsigned char nibble)
{
	if (nibble < 10) {
		return '0' + nibble;
	}

	return 'A' + (nibble - 10);
}

void print_x(unsigned char x)
{
	unsigned char hi = (((0xF0) & x) >> 4);
	unsigned char lo = (0x0F & x);
	Serial.print(_nibble_to_hex(hi));
	Serial.print(_nibble_to_hex(lo));
}

void print_eol(void)
{
	Serial.println();
}

#else

#include <stdio.h>
#include <stdlib.h>

void print_s(const char *str)
{
	printf("%s", str);
}

void print_f(float f)
{
	printf("%g", f);
}

void print_z(size_t z)
{
	printf("%zu", z);
}

void print_x(unsigned char x)
{
	printf("%02X", x);
}

void print_eol(void)
{
	printf("\n");
}

int main(int argc, char **argv)
{
	printf("sizeof(float) == %zu\n\n", sizeof(float));

	float original = argc > 1 ? strtof(argv[1], NULL) : 2.0 / 3.0;

	return check_round_trip(original);
}
#endif
