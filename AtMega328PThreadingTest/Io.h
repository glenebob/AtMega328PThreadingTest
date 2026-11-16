#pragma once

#include <stdint.h>
#include <stdio.h>

#include "Types.h"

#define IO_INACTIVE 0
#define IO_READ 1
#define IO_WRITE 2

typedef struct
{
    uint8_t flags;
} device_t;

typedef size_t (*device_read_f)(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);
typedef size_t (*device_write_f)(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout);
typedef bool (*device_seek_f)(device_t *device, int16_t offset, int8_t whence);
typedef size_t (*io_read_f)(void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
typedef size_t (*io_write_f)(const void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
typedef int (*io_seek_f)(FILE *stream, long offset, int whence);

typedef struct
{
    uint8_t status;
    uint8_t *buffer;
    uint8_t buffer_length;
    uint8_t offset;
    uint8_t timer_id;
    uint8_t thread_id;
} io_task_t;

size_t device_read(device_t *device, uint8_t *buffer, uint8_t count, uint16_t timeout);
size_t device_write(device_t *device, const uint8_t *buffer, uint8_t count, uint16_t timeout);
bool device_seek(device_t *device, size_t offset, int8_t whence);
int file_getchar(FILE *stream);
int file_putchar(char c, FILE *stream);
size_t file_read(void *buffer, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
size_t file_write(const void *buffer, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
int file_seek(FILE *stream, long offset, int whence);
size_t freadt(void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
size_t fwritet(const void *ptr, size_t size, size_t nmemb, FILE *stream, uint16_t timeout);
int io_dispatch(io_task_t *io_task, uint16_t timeout);
