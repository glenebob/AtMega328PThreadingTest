#pragma once

#include <stdio.h>

#include "Io.h"

device_t *open_eeprom_device(void);
FILE *open_eeprom_file(device_t *device);
