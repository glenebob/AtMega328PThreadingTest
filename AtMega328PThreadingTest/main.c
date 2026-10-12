#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/portpins.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "Types.h"
#include "Interrupt.h"
#include "Task.h"
#include "Timer.h"
#include "Thread.h"
#include "AsyncIo.h"

static const __flash char string_iterations[] PROGMEM = "%s: Iterations: %lu, Read: %lu, Write: %lu, Idle Spins: %lu\n";

static const __flash char string_wheee[] PROGMEM =
    "Begin big single write\r\n"
    "Wheeeeeeeeeeeeeeeeeeeeeee"
    "eeeeeeeeeeeeeeeeeeeeeeeee\r\n"
    "eeeeeeeeeeeeeeeeeeeeeeeee"
    "eeeeeeeeeeeeeeeeeeeeeeeee\r\n"
    "eeeeeeeeeeeeeeeeeeeeeeeee"
    "eeeeeeeeeeeeeeeeeeeeeeeee\r\n"
    "eeeeeeeeeeeeeeeeeeeeeeeee"
    "eeeeeeeeeeeeeeeeeeeeeeeee\r\n"
    "End big single write\r\n";

void thread_root_entry(void*);

void kernel_init_task(void *unused_arguments)
{
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    timers_initialize();
    io_task_init();

    thread_create(thread_root_entry, NULL, true);

    TCCR0A = (1 << WGM01);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);
}

void post_kernel_task(task_handler_t handler, void *arguments)
{
    INTERRUPT_RAISE_LEVEL();

    task_queue_push(handler, arguments);
    thread_queue(THREAD_ID_KERNEL);
    thread_yield_dispatch();
}

void thread_3_entry(void*);
void thread_4_entry(void*);
void thread_5_entry(void*);

__attribute__((OS_task))
void thread_root_entry(void *unused_arguments)
{
    static const __flash char string_root[] PROGMEM = "Root!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_root) - 1];

    memcpy_P(status, string_root, sizeof(status));

    memcpy_P(__malloc_heap_end - 12, PSTR("<malloc end>"), 12);

    DDRB |= (1 << PB5); 

    thread_create(thread_3_entry, NULL, true);
    thread_create(thread_4_entry, NULL, true);

    while (trueFlag)
    {
        thread_sleep(123);

        PINB = (1 << PB5);
    }

    memset(status, 0, sizeof(status));
}

__attribute__((OS_task))
void thread_3_entry(void *unused_arguments)
{
    static const __flash char string_thread_3[] PROGMEM = "Thread 3!";

    static volatile bool trueFlag = true;

    const size_t sizeof_status = sizeof(string_thread_3) - 1;

    char status[sizeof_status];

    memcpy_P(status, string_thread_3, sizeof_status);

    thread_create(thread_5_entry, NULL, true);

    while (trueFlag)
    {
        thread_sleep(150);

        PINB = (1 << PB5);
    }

    memset(status, 0, sizeof_status);
}

void test_kernel_task(void *unused_arguments)
{
    uint32_t read_count = 0;
    uint32_t write_count = 0;

    write_count += printf_P(PSTR("Kernel Thread starting...\n"));

    for (uint32_t i = 0; i < 5; ++i)
    {
        thread_sleep(250);

        PINB = (1 << PB5);

        write_count += printf_P(string_iterations, "Kernel Thread", i, read_count, write_count, idle_spin_count);
    }

    thread_sleep(250);

    printf_P(PSTR("Kernel Thread exiting...\n"));
}

__attribute__((OS_task))
void thread_4_entry(void *unused_arguments)
{
    static const __flash char string_thread_4[] PROGMEM = "Thread 4!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_thread_4) - 1];

    memcpy_P(status, string_thread_4, sizeof(status));

    set_getchar_timeout(5000);

    printf("\n");

    for (int i = 0; i < 60; ++i)
    {
        printf(".");
        thread_sleep(5);
    }        

    printf_P(PSTR(" Let's go!\n"));

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

            printf_P(PSTR("Allocate, copy, print, and free %u bytes...\n"), sizeof(string_wheee));

            uint8_t *dynamic = malloc(sizeof(string_wheee));

            if (!dynamic)
            {
                exit(ERR_MALLOC_FAILED);
            }

            memcpy_P(dynamic, string_wheee, sizeof(string_wheee));

            io_write(dynamic, (uint8_t) sizeof(string_wheee), 0);

            memset(dynamic, '/', sizeof(string_wheee));
            free(dynamic);

            thread_sleep(1000);

            printf_P(PSTR("Kernel task...\n"));

            post_kernel_task(test_kernel_task, NULL);

            printf_P(PSTR("That was fun.\n"));

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

            thread_sleep(1000);
        }

        length = printf_P(string_iterations, "Thread 4", i, read_count, write_count, idle_spin_count);

        write_count += length;

        thread_sleep(250);
    }

    memset(status, 0, sizeof(status));
}

__attribute__((OS_task))
void thread_5_entry(void *unused_arguments)
{
    static const __flash char string_thread_5[] PROGMEM = "Thread 5!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_thread_5) - 1];

    memcpy_P(status, string_thread_5, sizeof(status));

    int multiply1;
    int multiply2;
    int multiply3;

    while (trueFlag)
    {
        do
        {
            multiply1 = rand();
            multiply2 = rand();
            multiply3 = multiply1 * multiply2;

            thread_sleep(5);
        }
        while (multiply3);
    }

    memset(status, 0, sizeof(status));
}

__attribute__((OS_main))
int main()
{
    static const __flash char string_kernel[] PROGMEM = "Kernel!";

    static volatile bool trueFlag = true;

    char status[sizeof(string_kernel) - 1];

    memcpy_P(status, string_kernel, sizeof(status));

    interrupt_initialize();
    task_queue_initialize();
    threads_initialize();

    // Queue up remaining initialization.
    task_queue_push(kernel_init_task, NULL);

    INTERRUPT_LOWER_LEVEL();

    while (trueFlag)
    {
        task_handler_t handler;
        void *arguments;

        while (task_queue_pop(&handler, &arguments))
        {
            static const __flash char string_kernel_task[] PROGMEM = "Kernel Task!";

            char status[sizeof(string_kernel_task) - 1];

            memcpy_P(status, string_kernel_task, sizeof(status));

            handler(arguments);

            memset(status, 0, sizeof(status));
        }

        INTERRUPT_RAISE_LEVEL();

        thread_kernel_dormant();
        thread_yield_dispatch();
    }

    memset(status, 0, sizeof(status));
}
