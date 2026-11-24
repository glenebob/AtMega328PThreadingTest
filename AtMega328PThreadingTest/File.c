#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "F_CPU.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "Types.h"
#include "Math.h"
#include "Thread.h"
#include "Io.h"

#include "File.h"

typedef struct
{
    device_t *device;
} file_info_rw_t;

typedef struct
{
    device_t *device;
    uint16_t position;
} file_info_seek_t;

typedef struct {
    char	*buf;		/* buffer pointer */
    unsigned char unget;	/* ungetc() buffer */
    uint8_t	flags;		/* flags, see below */
    uint16_t	size;		/* size of buffer */
    int	len;		/* characters read or written so far */
    int	(*put)(char, struct __file *);	/* function to write one char to device */
    int	(*get)(struct __file *);	/* function to read one char from device */
    file_info_seek_t file_info;
} file_t;

static int file_getchar(FILE *stream);
static int file_putchar(char c, FILE *stream);

file_t stdin_file;
file_t stdout_err_file;

static uint16_t default_read_timeout;

int fclose(FILE *file)
{
    // TODO: Implement this completely.
    // The underlying device must be notified so it can manage its
    // memory and reference count.

    if (!file)
    {
        return -1;
    }

    thread_lock();

    if (file->flags & __SMALLOC)
    {
        free(file);
    }

    thread_unlock();

    return 0;
}

FILE *create_file(device_t *device)
{
    if (!device)
    {
        return NULL;
    }

    thread_lock();

    uint8_t device_flags;
    uint16_t device_size;
    uint8_t file_flags = __SMALLOC;
    int (*file_putc)(char, FILE *) = NULL;
    int (*file_getc)(FILE *) = NULL;

    device->function_table->info(device, &device_flags, &device_size);

    if (device_flags & DEVICE_READ)
    {
        file_flags |= _FDEV_SETUP_READ;
        file_getc = file_getchar;
    }

    if (device_flags & DEVICE_WRITE)
    {
        file_flags |= _FDEV_SETUP_WRITE;
        file_putc = file_putchar;
    }

    FILE *file;
    uint8_t file_info_size =
        device_flags & DEVICE_SEEK ?
        sizeof(file_info_seek_t) :
        sizeof(file_info_rw_t);

    file = malloc(sizeof(FILE) - sizeof(void*) + file_info_size);

    if (!file)
    {
        thread_unlock();

        return NULL;
    }

    fdev_setup_stream(file, file_putc, file_getc, file_flags);

    file->size = device_size;

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;

    file_info->device = device;

    if (device_flags & DEVICE_SEEK)
    {
        file_info->position = 0;
    }

    thread_unlock();

    return file;
}

void stdio_initialize(device_t *device)
{
    fdev_setup_stream((FILE*) &stdin_file, NULL, file_getchar, _FDEV_SETUP_READ);

    stdin_file.size = 0;
    stdin_file.file_info.device = device;

    fdev_setup_stream((FILE*) &stdout_err_file, file_putchar, NULL, _FDEV_SETUP_WRITE);

    stdout_err_file.size = 0;
    stdout_err_file.file_info.device = device;

    stdin = (FILE*) &stdin_file;
    stdout = (FILE*) &stdout_err_file;
    stderr = (FILE*) &stdout_err_file;
};

void stdio_crash_init(device_t *device)
{
    fdev_setup_stream((FILE*) &stdin_file, NULL, NULL, 0);
    fdev_setup_stream((FILE*) &stdout_err_file, file_putchar, NULL, _FDEV_SETUP_WRITE);

    stdout_err_file.size = 0;
    stdout_err_file.file_info.device = device;

    stdin = NULL;
    stdout = NULL;
    stderr = (FILE*) &stdout_err_file;
}

void stdin_set_getchar_timeout(uint16_t timeout)
{
    default_read_timeout = timeout;
}

static int read_write_result_to_get_put_result(size_t result, char c)
{
    if (result == -1)
    {
        return _FDEV_ERR;
    }
    else if (result == 0)
    {
        return _FDEV_EOF;
    }
    else
    {
        return c;
    }
}

static int file_getchar(FILE *file)
{
    if (!file)
    {
        return _FDEV_ERR;
    }

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;
    char c;
    size_t result = device_read(file_info->device, file_info->position, (uint8_t*) &c, 1, default_read_timeout);

    if (file->size && result != (size_t) -1)
    {
        file_info->position += result;
    }

    return read_write_result_to_get_put_result(result, c);
}

static int file_putchar(char c, FILE *file)
{
    if (!file)
    {
        return _FDEV_ERR;
    }

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;
    size_t result = device_write(file_info->device, file_info->position, (uint8_t*) &c, 1, default_read_timeout);

    if (file->size && result != (size_t) -1)
    {
        file_info->position += result;
    }

    return read_write_result_to_get_put_result(result, 0);
}

size_t fread(void *buffer, size_t size, size_t nmemb, FILE *file)
{
    return freadt(buffer, size, nmemb, file, default_read_timeout);
}

size_t freadt(void *buffer, size_t size, size_t nmemb, FILE *file, uint16_t timeout)
{
    if (!file)
    {
        return -1;
    }

    if (!(file->flags & __SRD))
    {
        return -1;
    }

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;
    uint16_t count = min_size(size * nmemb, 0xFF);

    size_t result = device_read(file_info->device, file_info->position, buffer, (uint8_t) count, timeout);

    if (result == -1)
    {
        return result;
    }

    if (file->size)
    {
        file_info->position += result;
    }

    return result / size;
}

size_t fwrite(const void *buffer, size_t size, size_t nmemb, FILE *file)
{
    return fwritet(buffer, size, nmemb, file, 0);
}

size_t fwritet(const void *buffer, size_t size, size_t nmemb, FILE *file, uint16_t timeout)
{
    if (!file)
    {
        return -1;
    }

    if (!(file->flags & __SWR))
    {
        return -1;
    }

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;
    uint16_t count = min_size(size * nmemb, 0xFF);

    size_t result = device_write(file_info->device, file_info->position, buffer, (uint8_t) count, timeout);

    if (result == -1)
    {
        return result;
    }

    if (file->size)
    {
        file_info->position += result;
    }

    return result / size;
}

int fseek(FILE *file, long offset, int whence)
{
    if (!file)
    {
        return -1;
    }

    if (!file->size)
    {
        return -1;
    }

    file_info_seek_t *file_info = (file_info_seek_t*) &file->udata;

    switch (whence)
    {
        case SEEK_SET:
            if (offset < 0 || offset > file->size)
            {
                return -1;
            }

            break;

        case SEEK_CUR:
        {
            offset = file_info->position + offset;

            if (offset < 0)
            {
                return -1;
            }
            else
            {
                if (offset > file->size)
                {
                    return -1;
                }
            }

            break;
        }

        case SEEK_END:
            if (offset > 0)
            {
                return -1;
            }

            offset = file->size + offset;

            if (offset < 0)
            {
                return -1;
            }

            break;

        default:
            return -1;
    }

    file_info->position = (uint16_t) offset;

    return 0;
}
