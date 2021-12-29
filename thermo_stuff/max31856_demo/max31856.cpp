/* https://datasheets.maximintegrated.com/en/ds/MAX31856.pdf */

#include "max31856.h"
#include <math.h>		/* NAN */

/* man 3 memset */
/* extern void *memset(void *, int, size_t); */

#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
void max31856_spi_transfer(void *context, uint8_t *buf0, size_t len0,
			   uint8_t *buf1, size_t len1)
{
	const uint32_t spi_clock = 1000000;
	const uint8_t spi_bit_order = MSBFIRST;
	const uint8_t spi_data_mode = SPI_MODE1;

	uint8_t *chip_select_pin_ptr = (uint8_t *)context;
	uint8_t chip_select_pin = *chip_select_pin_ptr;

	SPISettings spis = SPISettings(spi_clock, spi_bit_order, spi_data_mode);
	SPI.beginTransaction(spis);
	digitalWrite(chip_select_pin, LOW);

	SPI.transfer(buf0, len0);
	if (buf1 && len1) {
		SPI.transfer(buf1, len1);
	}

	digitalWrite(chip_select_pin, HIGH);
	SPI.endTransaction();
}

void (*max31856_data_transfer)(void *context, uint8_t *buf0, size_t len0,
			       uint8_t *buf1, size_t len1) =
    max31856_spi_transfer;

#else /* not on arduino set the function pointer from main(); */

void (*max31856_data_transfer)(void *context, uint8_t *buf0, size_t len0,
			       uint8_t *buf1, size_t len1) = NULL;
#endif /* #ifdef ARDUINO */

void max31856_data_write(void *context, uint8_t *buffer, size_t len)
{
	max31856_data_transfer(context, buffer, len, NULL, 0);
}

void max31856_data_read(void *context, uint8_t *buffer, size_t len)
{
	memset(buffer, 0xFF, len);
	max31856_data_transfer(context, buffer, len, NULL, 0);
}

void max31856_data_write_and_read(void *context, uint8_t *write_buf,
				  size_t write_len, uint8_t *read_buf,
				  size_t read_len)
{
	memset(read_buf, 0xFF, read_len);
	max31856_data_transfer(context, write_buf, write_len, read_buf,
			       read_len);
}

void max31856_write_register(void *context, uint8_t address, uint8_t value)
{
	address |= 0x80;
	const size_t buf_len = 2;
	uint8_t buf[2] = { address, value };

	max31856_data_write(context, buf, buf_len);
}

void max31856_read_register(void *context, uint8_t address,
			    uint8_t *read_buf, uint8_t len)
{
	const size_t write_len = 1;
	uint8_t write_buf[1] = { (uint8_t)(address & 0x7F) };
	max31856_data_write_and_read(context, write_buf, write_len, read_buf,
				     len);
}

uint8_t max31856_read_register_8(void *context, uint8_t addr)
{
	uint8_t ret = 0;
	max31856_read_register(context, addr, &ret, 1);
	return ret;
}

uint16_t max31856_read_register_16(void *context, uint8_t addr)
{
	const size_t buf_len = 2;
	uint8_t buffer[2] = { 0, 0 };
	max31856_read_register(context, addr, buffer, buf_len);
	return (((uint16_t)buffer[0]) << 8) + buffer[1];
}

uint32_t max31856_read_register_24(void *context, uint8_t addr)
{
	uint8_t buffer[3] = { 0, 0, 0 };
	max31856_read_register(context, addr, buffer, 3);

	return (((uint32_t)buffer[0]) << 16)
	    + (((uint32_t)buffer[1]) << 8)
	    + buffer[2];
}

void max31856_set_continuous(void *context, bool continuous)
{
	uint8_t config0 = 0x00;
	uint8_t val = max31856_read_register_8(context, config0);
	if (continuous) {
		/* enable continuous */
		val |= 0x80;
		/* disable "one shot" */
		val &= ~(0x40);
	} else {
		/* disable continuous */
		val &= ~(0x80);
		/* enable "one shot" */
		val |= (0x40);
	}
	max31856_write_register(context, config0, val);
}

void max31856_prepare_read(void *context)
{
	uint8_t config0 = 0x00;
	uint8_t val = max31856_read_register_8(context, 0x00);
	/* disable continuous */
	val &= ~(0x80);
	/* enable "one shot" */
	val |= 0x40;
	max31856_write_register(context, config0, val);
}

bool max31856_read_ready(void *context)
{

	uint8_t config0 = 0x00;
	uint8_t one_shot = 0x40;
	return !(max31856_read_register_8(context, config0) & one_shot);
}

void max31856_set_thermocouple_type_k(void *context)
{
	const uint8_t config1 = 0x01;
	const uint8_t type_k = 3;
	uint8_t val = max31856_read_register_8(context, config1);
	val &= 0xF0;
	val |= type_k & 0x0F;
	max31856_write_register(context, config1, val);
}

float max31856_read_c(void *context)
{
	if (!max31856_read_ready(context)) {
		return NAN;
	}

	uint8_t address = 0x0C;
	int32_t temp128 = max31856_read_register_24(context, address);

	/* if negative */
	if (temp128 & 0x800000) {
		/* fill the top byte with 0xFF */
		temp128 |= 0xFF000000;
	}
	/* shift off the non-temperature lower bits */
	temp128 >>= 5;

	float temp_c = temp128 * (1.0 / 128.0);
	return temp_c;
}
