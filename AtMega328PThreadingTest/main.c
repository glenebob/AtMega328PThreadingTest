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

#include "Types.h"
#include "Interrupt.h"
#include "Task.h"
#include "Timer.h"
#include "Thread.h"
#include "AsyncIo.h"

void thread_root_entry(void*);

static volatile uint32_t idle_spin_count = 0;

void thread_idle_entry(void *unused_arguments)
{
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_enable();

    for (;;)
    {
        INTERRUPT_RAISE_LEVEL();

        ++idle_spin_count;

        INTERRUPT_LOWER_LEVEL();

        sleep_cpu();
    }    
}    

void kernel_init_task(void *unused_arguments)
{
    // Create the idle thread (ID: 1).
    thread_create_unlinked(thread_idle_entry, NULL, false, THREAD_STATUS_PERMANENT);

    thread_create(thread_root_entry, NULL, true);

    TCCR0A = (1 << WGM01);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);
}

void sleep(uint16_t milliseconds)
{
    if (!milliseconds)
    {
        return;
    }
    
    INTERRUPT_RAISE_LEVEL();

    timer_add(current_thread_id, milliseconds);
    thread_block(current_thread_id);
    thread_yield_dispatch();
}

void post_kernel_task(task_handler_t handler, void *arguments)
{
    INTERRUPT_RAISE_LEVEL();

    task_queue_push(handler, arguments);
    thread_dispatch_to_kernel();
}

void thread_3_entry(void*);
void thread_4_entry(void*);

void thread_root_entry(void *unused_arguments)
{
    DDRB |= (1 << PB5); 

    thread_create(thread_3_entry, NULL, true);
    thread_create(thread_4_entry, NULL, true);

    for (;;)
    {
        sleep(123);

        PINB = (1 << PB5);
    }
}

void thread_3_entry(void *unused_arguments)
{
    for (int i = 0; i < 10; ++i)
    {
        void *dynamic = malloc(32);

        if (!dynamic)
        {
            exit(ERR_MALLOC_FAILED);
        }

        sleep(150);

        PINB = (1 << PB5);

        free(dynamic);
    }

    thread_create(thread_3_entry, NULL, true);
}

void test_kernel_task(void *unused_arguments)
{
    for (int i = 0; i < 20; ++i)
    {
        for (uint32_t i = 0; i < 50000; ++i);

        PINB = (1 << PB5);
    }
}

void thread_4_entry(void *unused_arguments)
{
    set_getchar_timeout(5000);

    printf("\n");

    for (int i = 0; i < 60; ++i)
    {
        printf(".");
        sleep(5);
    }        

    printf(" Let's go!\n");
    sleep(1000);

    uint32_t read_count = 0;
    uint32_t write_count = 0;
    char line_buffer[16];
    int length;

    for (uint32_t i = 0; ; ++i)
    {
        if ((i % 25) == 0)
        {
            printf("Kernel task...\n");

            post_kernel_task(test_kernel_task, NULL);

            printf("That was fun.\n");

            length = printf("Iterations: %lu, Read: %lu, Write: %lu, Idle Spins: %lu\n", i, read_count, write_count, idle_spin_count);

            printf("Now what say ye? >");

            if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL)
            {
                printf("\n<read timeout>\n");
            }
            else
            {
                length = strlen(line_buffer);
                
                read_count += length;

                printf("\nSaid ye: %s", line_buffer);
                
                if (line_buffer[length - 1] != '\n')
                {
                    printf(" <buffer overflow>\n");
                }
            }                

            sleep(1000);
        }

        length = printf("Iterations: %lu, Read: %lu, Write: %lu, Idle Spins: %lu\n", i, read_count, write_count, idle_spin_count);

        write_count += length;

        sleep(250);
    }
}

int main()
{
    INTERRUPT_INITIALIZE();

    timers_initialize();
    task_queue_init();
    threads_initialize(thread_idle_entry);
    io_task_init();

    current_thread_id = THREAD_ID_KERNEL;
    current_thread = thread_get(current_thread_id);

    task_queue_push(kernel_init_task, NULL);

    INTERRUPT_LOWER_LEVEL();

    for (;;)
    {
        task_handler_t handler;
        void *arguments;

        while (task_queue_pop(&handler, &arguments))
        {
            handler(arguments);
        }

        INTERRUPT_RAISE_LEVEL();

        thread_yield_dispatch();
    }
}
