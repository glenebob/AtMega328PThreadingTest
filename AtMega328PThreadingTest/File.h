#pragma once

#include <stdio.h>

#include "Io.h"

void stdio_initialize(device_t *device);
void stdio_crash_init(device_t *device);
void stdin_set_getchar_timeout(uint16_t timeout);
FILE *create_file(device_t *device);
size_t freadt(void *buffer, size_t size, size_t nmemb, FILE *file, uint16_t timeout);
size_t fwritet(const void *buffer, size_t size, size_t nmemb, FILE *file, uint16_t timeout);
