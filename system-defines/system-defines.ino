/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* system-defines.c : print common platform definitions */
/* Copyright (C) 2023 Eric Herman <eric@freesa.org> */

/*
  To compile and run as a native executable:
	gcc -x c ./system-defines.ino -o system-defines && ./system-defines
  or:
	g++ -x c++ ./system-defines.ino -o system-defines && ./system-defines
*/

#ifdef ARDUINO
#include <HardwareSerial.h>
#define print_s(s) Serial.print(s)
#define print_eol(void) Serial.println()
#define print_i(i) Serial.print(i)
#define print_l(l) Serial.print(l)
#define print_u(u) Serial.print(u)
#define print_ul(ul) Serial.print(ul)
#define print_ull(ull) Serial.print(ull)
#define print_z(z) Serial.print(z)
#define print_f(f) Serial.print((float)f)
#define print_lf(lf) Serial.print((double)lf)
/* The Arduino C++ class does not define Serial.print(long double) */
#define print_llf(llf) Serial.print((double)llf)
#else
#include <stdio.h>
#define print_s(s) printf("%s", s)
#define print_eol(void) printf("\n")
#define print_i(i) printf("%d", i)
#define print_l(l) printf("%ld", l)
#define print_u(u) printf("%u", u)
#define print_ul(ul) printf("%lu", ul)
#define print_ull(ull) printf("%llu", ull)
#define print_z(z) printf("%zu", z)
#define print_f(f) printf("%g", f)
#define print_lf(lf) printf("%g", lf)
#define print_llf(llf) printf("%Lg", llf)
#endif

#include <limits.h>
#include <stdint.h>
#include <float.h>
/*

	This function prints some of the more important parameters for
	the system

	Noteworthy is that not all toolchains define all of these.
	A gcc toolchain may not define __WORDSIZE for Arduino, for instance.

*/
void print_system_defines(void)
{
	print_s("_POSIX_C_SOURCE: ");
#ifdef _POSIX_C_SOURCE
	print_l(_POSIX_C_SOURCE);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("ARDUINO: ");
#ifdef ARDUINO
	print_i(ARDUINO);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("__STDC_HOSTED__: ");
#ifdef __STDC_HOSTED__
	print_i(__STDC_HOSTED__);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("__STDC_VERSION__: ");
#ifdef __STDC_VERSION__
	print_l(__STDC_VERSION__);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("__cplusplus: ");
#ifdef __cplusplus
	print_l(__cplusplus);
#else
	print_s("not defined");
#endif
	print_eol();

	print_eol();

	print_s("NDEBUG: ");
#ifdef NDEBUG
	print_i(NDEBUG);
	print_s(", runtime assert(x) will not be active");
#else
	print_s("not defined, runtime assert(x) WILL be active");
#endif
	print_eol();

	print_eol();

	print_s("__WORDSIZE: ");
#ifdef __WORDSIZE
	print_i(__WORDSIZE);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("CHAR_BIT: ");
#ifdef CHAR_BIT
	print_i(CHAR_BIT);
	if (CHAR_BIT != 8) {
		print_s(", expected '8', THIS IS VERY UNUSUAL!");
	}
#else
	print_s("not defined");
#endif
	print_eol();

	print_eol();

#ifdef CHAR_MIN
	print_s(CHAR_MIN == 0 ? "char is unsigned" : "char is signed");
	print_s(" (CHAR_MIN: ");
	print_i(CHAR_MIN);
#ifdef CHAR_MAX
	print_s(", CHAR_MAX: ");
	print_u(CHAR_MAX);
#endif
	print_s(")");
#else
	print_s("CHAR_MIN: ");
	print_s("not defined");
#endif
	print_eol();

	print_eol();

	print_s("        sizeof(unsigned char): ");
	print_z(sizeof(unsigned char));
	print_s(",\t UCHAR_MAX: ");
#ifdef UCHAR_MAX
	print_u(UCHAR_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("       sizeof(unsigned short): ");
	print_z(sizeof(unsigned short));
	print_s(",\t USHRT_MAX: ");
#ifdef UCHAR_MAX
	print_i(USHRT_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("         sizeof(unsigned int): ");
	print_z(sizeof(unsigned int));
	print_s(",\t  UINT_MAX: ");
#ifdef UCHAR_MAX
	print_u(UINT_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("        sizeof(unsigned long): ");
	print_z(sizeof(unsigned long));
	print_s(",\t ULONG_MAX: ");
#ifdef UCHAR_MAX
	print_ul(ULONG_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("   sizeof(unsigned long long): ");
	print_z(sizeof(unsigned long long));
	print_s(",\tULLONG_MAX: ");
#ifdef ULLONG_MAX
	print_ull(ULLONG_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("               sizeof(size_t): ");
	print_z(sizeof(size_t));
	print_s(",\t  SIZE_MAX: ");
#ifdef UCHAR_MAX
	print_z(SIZE_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("               sizeof(float)): ");
	print_z(sizeof(float));
	print_s(",\t   FLT_MAX: ");
#ifdef FLT_MAX
	print_f(FLT_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("              sizeof(double)): ");
	print_z(sizeof(double));
	print_s(",\t   DBL_MAX: ");
#ifdef DBL_MAX
	print_lf(DBL_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_s("         sizeof(long double)): ");
	print_z(sizeof(long double));
	print_s(",\t  LDBL_MAX: ");
#ifdef LDBL_MAX
	print_llf(LDBL_MAX);
#else
	print_s("not defined");
#endif
	print_eol();

	print_eol();

	print_s("void pointers and function pointers are");
	if (sizeof(void *) != sizeof(&print_system_defines)) {
		print_s(" NOT");
	}
	print_s(" the same size:");

	print_eol();

	print_s("                sizeof(void*)): ");
	print_z(sizeof(void *));
	print_eol();

	print_s(" sizeof(&print_system_defines): ");
	print_z(sizeof(&print_system_defines));
	print_eol();
}

/*

	Below here is the platform specific code to allow creation of either
	an arduino-style "sketch" (typically a .ino file), or a standard
	standard executable as would be expected on a __STDC_HOSTED__ system
	like a POSIX or DOS or MS Windows machine.

*/
#ifdef ARDUINO
void setup(void)
{
	Serial.begin(9600);
	delay(50);

	Serial.println();
	Serial.println();

	Serial.println("Begin");

	Serial.println();
}

void loop(void)
{
	Serial.println();
	print_system_defines();
	uint16_t delay_ms = (5 * 1000);
	delay(delay_ms);
}
#else /* #ifdef ARDUINO */
int main(void)
{
	print_system_defines();
	return 0;
}
#endif /* #ifdef ARDUINO */
