#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "Types.h"
#include "Error.h"
#include "Bits.h"
#include "Math.h"
#include "Thread.h"
#include "UartDevice.h"
#include "Mutex.h"
#include "Event.h"
#include "Io.h"
#include "EepromDevice.h"
#include "NullDevice.h"
#include "FlashDevice.h"
#include "File.h"

static const __flash char string_iterations[] = "%s: Iterations: %lu, Read: %lu, Write: %lu, Idle Spins: %lu\n";

static const __flash char string_wheee[] =
    "Begin big single write\n"
    "Wheeeeeeeeeeeeeeeeeeeeee!\n"
    "End big single write\n";

mutex_t io_mutex;
event_t test_event;

void thread_root_entry(void*);
void thread_3_entry(void*);
void thread_4_entry(void*);
void thread_5_entry(void*);
void test_kernel_task(void *unused_arguments);

extern uint8_t _end;

FILE *null_file;
FILE *zero_file;
FILE *eeprom_file;
FILE *flash_file;

__attribute__((OS_main))
int main(void)
{
    for (uint8_t *address = &_end; address <= (uint8_t*) 0x08FD; ++address)
    {
        *address = 0;
    }

    static const __flash char string_main[] = "Main!";

    char status[sizeof(string_main) - 1];

    memcpy_P(status, string_main, sizeof(status));

    DDRB |= BIT5;
    PORTB &= ~BIT5;

    thread_kernel_entry(thread_root_entry);

    exit(ERR_KERNEL_THREAD_RETURNED);
}

__attribute__((OS_task))
void thread_root_entry(void *unused_arguments)
{
    static const __flash char string_root[] = "Root!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_root) - 1];

    memcpy_P(status, string_root, sizeof(status));

    memcpy_P(__malloc_heap_end - 12, PSTR("<malloc end>"), 12);

    uart_initialize(UART_BAUD, UART_FLAGS);
    eeprom_initialize();
    mutex_initialize(&io_mutex);
    event_initialize(&test_event, EVENT_AUTO_RESET);

    stdio_initialize((device_t*) uart_device);
    null_file = create_file((device_t*) null_device);
    zero_file = create_file((device_t*) zero_device);
    eeprom_file = create_file(eeprom_device);
    flash_file = create_file((device_t*) flash_device);

    fgetc(null_file);
    fputc(5, null_file);
    fgetc(zero_file);
    fputc(5, zero_file);

    fprintf_P(eeprom_file, PSTR("Here we go!\n"));
    fseek(eeprom_file, 0, SEEK_SET);

    thread_lock();
    uint8_t *buffer = malloc(32);
    uint16_t remaining = 0x8000;
    thread_unlock();

    while (remaining)
    {
        uint16_t bytes_read = fread(buffer, 1, min_size(32, remaining), flash_file);
        remaining -= bytes_read;
    }

    thread_lock();
    free(buffer);
    thread_unlock();

    printf("\n");

    for (int i = 0; i < 60; ++i)
    {
        printf(".");
        thread_sleep(5);
    }

    printf_P(PSTR(" Let's go!\n"));

    thread_create(thread_3_entry, NULL, true);
    thread_create(thread_4_entry, NULL, true);

    thread_sleep(500);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread Root: Triggering event...\n"));
    mutex_unlock(&io_mutex);

    event_trigger(&test_event);

    while (trueFlag)
    {
        thread_sleep(123);

        PINB = BIT5;
    }

    memset(status, 0, sizeof(status));
}

__attribute__((OS_task))
void thread_3_entry(void *unused_arguments)
{
    static const __flash char string_thread_3[] = "Thread 3!";

    static volatile bool trueFlag = true;

    const size_t sizeof_status = sizeof(string_thread_3) - 1;

    char status[sizeof_status];

    memcpy_P(status, string_thread_3, sizeof_status);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread 3: Waiting for event...\n"));
    mutex_unlock(&io_mutex);

    event_wait(&test_event);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread 3: Event triggered!\n"));
    mutex_unlock(&io_mutex);

    thread_create(thread_5_entry, NULL, true);

    thread_sleep(1000);

    while (current_thread_priority == 1);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread 3: Triggering event!\n"));
    mutex_unlock(&io_mutex);

    event_trigger(&test_event);

    while (trueFlag)
    {
        thread_sleep(150);

        PINB = BIT5;
    }

    memset(status, 0, sizeof_status);
}

void test_kernel_task(void *unused_arguments)
{
    uint32_t read_count = 0;
    uint32_t write_count = 0;

    mutex_lock(&io_mutex);
    write_count += printf_P(PSTR("Kernel Task starting...\n"));
    mutex_unlock(&io_mutex);

    for (uint32_t i = 0; i < 5; ++i)
    {
        thread_sleep(250);

        PINB = BIT5;

        mutex_lock(&io_mutex);
        write_count += printf_P(string_iterations, "Kernel Task", i, read_count, write_count, idle_spin_count);
        mutex_unlock(&io_mutex);
    }

    thread_sleep(250);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Kernel Task exiting...\n"));
    mutex_unlock(&io_mutex);
}

__attribute__((OS_task))
void thread_4_entry(void *unused_arguments)
{
    static const __flash char string_thread_4[] = "Thread 4!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_thread_4) - 1];

    memcpy_P(status, string_thread_4, sizeof(status));

    stdin_set_getchar_timeout(5000);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread 4: Waiting for event...\n"));
    mutex_unlock(&io_mutex);

    event_wait(&test_event);

    mutex_lock(&io_mutex);
    printf_P(PSTR("Thread 4: Event triggered!\n"));
    mutex_unlock(&io_mutex);

    thread_sleep(1000);

    uint32_t read_count = 0;
    uint32_t write_count = 0;
    char line_buffer[16];
    int length;

    for (uint32_t i = 0; trueFlag; ++i)
    {
        if ((i % 9) == 0)
        {
            thread_sleep(1000);

            mutex_lock(&io_mutex);
            printf_P(PSTR("Allocate, copy, print, and free %u bytes...\n"), sizeof(string_wheee));

            uint8_t *dynamic = malloc(sizeof(string_wheee));
            mutex_unlock(&io_mutex);

            if (!dynamic)
            {
                exit(ERR_MALLOC_FAILED);
            }

            memcpy_P(dynamic, string_wheee, sizeof(string_wheee));

            mutex_lock(&io_mutex);
            fwrite(dynamic, sizeof(uint8_t), sizeof(string_wheee), stdout);
            mutex_unlock(&io_mutex);

            memset(dynamic, '/', sizeof(string_wheee));

            thread_lock();
            free(dynamic);
            thread_unlock();

            thread_sleep(1000);

            mutex_lock(&io_mutex);
            printf_P(PSTR("Kernel task...\n"));
            mutex_unlock(&io_mutex);

            thread_post_kernel_task(test_kernel_task, NULL);

            mutex_lock(&io_mutex);
            length = printf_P(string_iterations, "Thread 4", i, read_count, write_count, idle_spin_count);
            printf_P(PSTR("Now what say ye? >"));

            if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL)
            {
                printf_P(PSTR("\n<read timeout>\n"));
            }
            else
            {
                length = strlen(line_buffer);
                
                read_count += length;

                printf_P(PSTR("\nSaid ye: %s"), line_buffer);
                
                if (line_buffer[length - 1] != '\n')
                {
                    printf_P(PSTR(" <buffer overflow>\n"));
                }
            }

            mutex_unlock(&io_mutex);

            thread_sleep(1000);

            mutex_lock(&io_mutex);
            printf_P(PSTR("Thread 4: Waiting for event...\n"));
            mutex_unlock(&io_mutex);

            event_wait(&test_event);

            mutex_lock(&io_mutex);
            printf_P(PSTR("Thread 4: Event triggered!\n"));
            mutex_unlock(&io_mutex);
        }

        mutex_lock(&io_mutex);
        fprintf_P(eeprom_file, PSTR("%s: Iterations: %lu\n"), "Thread 4", i);
        fseek(eeprom_file, 0, SEEK_SET);
        mutex_unlock(&io_mutex);

        mutex_lock(&io_mutex);
        length = printf_P(string_iterations, "Thread 4", i, read_count, write_count, idle_spin_count);
        mutex_unlock(&io_mutex);

        write_count += length;

        thread_sleep(250);
    }

    memset(status, 0, sizeof(status));
}

static volatile uint32_t j;

__attribute__((OS_task))
void thread_5_entry(void *unused_arguments)
{
    static const __flash char string_thread_5[] = "Thread 5!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_thread_5) - 1];

    uint32_t write_count = 0;
    int length;
    char line_buffer[32];

    memcpy_P(status, string_thread_5, sizeof(status));

    int multiply1;
    int multiply2;
    int multiply3;

    for (uint32_t i = 0; trueFlag; ++i)
    {
        for (j = 0; j < 3000; ++j)
        {
            multiply1 = rand();
            multiply2 = rand();
            multiply3 = multiply1 * multiply2;
        }

        do
        {
            multiply1 = rand();
            multiply2 = rand();
            multiply3 = multiply1 * multiply2;

            thread_sleep(5);
        }
        while ((uint32_t) multiply3 > 500);

        mutex_lock(&io_mutex);
        mutex_lock(&io_mutex);
        mutex_lock(&io_mutex);
        mutex_lock(&io_mutex);

        length = printf_P(string_iterations, "Thread 5", i, (uint32_t) 0, write_count, idle_spin_count);

        mutex_unlock(&io_mutex);
        mutex_unlock(&io_mutex);
        mutex_unlock(&io_mutex);
        mutex_unlock(&io_mutex);

        write_count += length;

        thread_sleep(250);

        mutex_lock(&io_mutex);
        printf_P(PSTR("Thread 5: Triggering event...\n"));
        mutex_unlock(&io_mutex);

        event_trigger(&test_event);

        mutex_lock(&io_mutex);
        fgets(line_buffer, sizeof(line_buffer), eeprom_file);
        fseek(eeprom_file, 0, SEEK_SET);
        printf_P(PSTR("Thread 5: Read from eeprom: %s"), line_buffer);
        mutex_unlock(&io_mutex);
    }

    memset(status, 0, sizeof(status));
}
