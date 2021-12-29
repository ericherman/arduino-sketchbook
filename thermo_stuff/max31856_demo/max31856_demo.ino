#include <Arduino.h>
#include <SPI.h>

/* https://datasheets.maximintegrated.com/en/ds/MAX31856.pdf */
#include "max31856.h"

/* INTERNAL PINS */
uint8_t thermo_couple_chip_select_pin = 2;
void *max31856_contxt = &thermo_couple_chip_select_pin;

void setup(void)
{
	Serial.begin(9600);
	delay(50);

	Serial.println("MAX31856 thermocouple");

	pinMode(thermo_couple_chip_select_pin, OUTPUT);
	digitalWrite(thermo_couple_chip_select_pin, HIGH);

	SPI.begin();

	max31856_set_thermocouple_type_k(max31856_contxt);

	max31856_set_continuous(max31856_contxt, false);
}

void loop(void)
{
	max31856_prepare_read(max31856_contxt);

	delay(500);

	float degrees = max31856_read_c(max31856_contxt);
	Serial.println(degrees);
}
