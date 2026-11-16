#pragma once

#include <stdio.h>

#include "Io.h"

extern device_t *eeprom_device;
extern FILE *eeprom_file;

void eeprom_initialize(void);
