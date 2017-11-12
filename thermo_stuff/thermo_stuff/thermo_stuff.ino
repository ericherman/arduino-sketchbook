#include <Arduino.h>
#include <SPI.h>

#define TTY_BAUD 9600
#define CHIP_SELECT_PIN 10
#define LED_PIN 8

#define Eprint(val) Serial.print(val)
#define Eprintln(val) Serial.println(val)

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

uint32_t count = 0;
void loop()
{
	uint32_t raw;
	struct max31855_s smax;

	++count;
	digitalWrite(LED_PIN, ((count % 2) == 0) ? LOW : HIGH);

	raw = spi_read_big_endian_uint32(CHIP_SELECT_PIN);

	max31855_from_u32(&smax, raw);

	if (smax.fault) {
		debug_max31855(raw);
		delay(2000);
		return;
	}

	Serial.print(count);
	Serial.print(", ");

	Serial.print(max31855_degrees(&smax));
	Serial.print(", ");
	Serial.println(max31855_internal(&smax));
	delay(450);
}
