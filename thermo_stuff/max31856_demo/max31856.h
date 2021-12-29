/* https://datasheets.maximintegrated.com/en/ds/MAX31856.pdf */

#ifndef MAX31856_H
#define MAX31856_H 1

#ifdef __cplusplus
#define Max31856_begin_C_functions extern "C" {
#define Max31856_end_C_functions }
#else
#define Max31856_begin_C_functions
#define Max31856_end_C_functions
#endif

Max31856_begin_C_functions
#undef Max31856_begin_C_functions
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
void max31856_set_continuous(void *context, bool continuous);

void max31856_prepare_read(void *context);

bool max31856_read_ready(void *context);

void max31856_set_thermocouple_type_k(void *context);

float max31856_read_c(void *context);

extern void (*max31856_data_transfer)(void *context, uint8_t *buf0, size_t len0,
				      uint8_t *buf1, size_t len1);

Max31856_end_C_functions
#undef Max31856_end_C_functions
#endif /* MAX31856_H */
