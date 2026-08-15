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
#include "Assembly.h"

void yield();
void thread_root_entry(void*);

uint32_t idle_spin_count = 0;

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
    thread_create(thread_root_entry, NULL, true);

    TCCR0A = (1 << WGM01);
    OCR0A = 249;
    TIMSK0 |= (1 << OCIE0A);
    TCCR0B = (1 << CS01) | (1 << CS00);
}

void thread_kernel_task_entry(void *unused_arguments)
{
    task_queue_push(kernel_init_task, NULL);

    for (;;)
    {
        task_handler_t handler;
        void *arguments;

        while (task_queue_pop(&handler, &arguments))
        {
            handler(arguments);
        }

        yield();
    }
}

void yield();
void save_context();
void initialize_or_restore_context_and_dispatch();
void initialize_context_and_dispatch();
void restore_context_and_dispatch();

void thread_entry();

__attribute__((naked))
void thread_entry()
{
    current_thread->entry_point(current_thread->argument);
    
    cli();
    
    thread_remove_from_run_queue(current_thread_id);
    thread_delete(current_thread_id);

    if (runnable_thread_count)
    {
        current_thread_id = thread_get_first_runnable();
    }
    else
    {
        current_thread_id = 0;
    }
    
    current_thread = thread_get(current_thread_id);

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    INIT_OR_RESTORE_CONTEXT();
    DISPATCH();
}

__attribute__((naked))
void yield_dispatch()
{
    SAVE_CONTEXT();

    current_thread_id = thread_get_first_runnable();
    current_thread = thread_get(current_thread_id);

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    INIT_OR_RESTORE_CONTEXT();
    DISPATCH();
}

void yield()
{
    cli();

    if (current_thread_id == 1)
    {
        if (!runnable_thread_count)
        {
            sei();
            return;
        }
    } else {
        if (runnable_thread_count == 1)
        {
            sei();
            return;
        }
    }        

    if (current_thread_id != 1)
    {
        thread_move_first_to_end();
    }
        
    yield_dispatch();
}

__attribute__((naked))
void sleep_dispatch()
{
    SAVE_CONTEXT();

    if (runnable_thread_count)
    {
        current_thread_id = thread_get_first_runnable();
    }
    else
    {
        current_thread_id = 0;
    }

    current_thread = thread_get(current_thread_id);

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }

    INIT_OR_RESTORE_CONTEXT();
    DISPATCH();
}

void sleep(uint16_t milliseconds)
{
    if (!milliseconds)
    {
        return;
    }
    
    cli();

    thread_remove_from_run_queue(current_thread_id);
    timer_add(current_thread_id, milliseconds);

    sleep_dispatch();
}

void thread_1_entry(void*);
void thread_2_entry(void*);
void thread_3_entry(void*);

void thread_root_entry(void *unused_arguments)
{
    DDRB |= (1 << PB5); 

    thread_create(thread_1_entry, NULL, true);
    thread_create(thread_2_entry, NULL, true);
    thread_create(thread_3_entry, NULL, true);

    for (;;)
    {
        sleep(123);

        PINB = (1 << PB5);
    }
}

void thread_1_entry(void *unused_arguments)
{
    for (;;)
    {
        sleep(150);

        PINB = (1 << PB5);
    }        
}

void thread_2_entry(void *unused_arguments)
{
    set_getchar_timeout(5000);

    sleep(5);

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
            printf("What say ye>");

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

void thread_3_entry(void *unused_arguments)
{
    sleep(10000);
}

__attribute__((naked))
int main()
{
    INTERRUPT_INITIALIZE();

    timers_initialize();
    task_queue_init();
    threads_initialize(thread_idle_entry, thread_kernel_task_entry);
    io_task_init();

    current_thread_id = 1;
    current_thread = thread_get(current_thread_id);
    
    INIT_CONTEXT();
    DISPATCH();
    
    return 1;
}

bool need_to_switch_threads()
{
    if (runnable_thread_count)
    {
        if (current_thread_id == 0)
        {
            return true;
        }
        else if (current_thread->ticks != 0xFF)
        {
            if (current_thread->ticks)
            {
                --current_thread->ticks;
            }

            if (!current_thread->ticks && runnable_thread_count > 1)
            {
                return true;
            }
        }
    }
        
    return false;
}

__attribute__((naked))
ISR(TIMER0_COMPA_vect)
{
    asm volatile (
        "push r0                  \n\t"
        "in   r0, __SREG__        \n\t"
        "push r0                  \n\t"
        "push r1                  \n\t"
        "clr  __zero_reg__        \n\t"
        "push r18                 \n\t"
        "push r19                 \n\t"
        "push r20                 \n\t"
        "push r21                 \n\t"
        "push r22                 \n\t"
        "push r23                 \n\t"
        "push r24                 \n\t"
        "push r25                 \n\t"
        "push r26                 \n\t"
        "push r27                 \n\t"
        "push r30                 \n\t"
        "push r31                 \n\t"
        ::
    );

    advance_active_timers();

    if (!need_to_switch_threads())
    {
        asm volatile (
            "pop  r31                 \n\t"
            "pop  r30                 \n\t"
            "pop  r27                 \n\t"
            "pop  r26                 \n\t"
            "pop  r25                 \n\t"
            "pop  r24                 \n\t"
            "pop  r23                 \n\t"
            "pop  r22                 \n\t"
            "pop  r21                 \n\t"
            "pop  r20                 \n\t"
            "pop  r19                 \n\t"
            "pop  r18                 \n\t"
            "pop  r1                  \n\t"
            "pop  r0                  \n\t"
            "out  __SREG__, r0        \n\t"
            "pop  r0                  \n\t"
            "reti                     \n\t"
            ::
        );
    }
    else
    {
        asm volatile (
            "pop  r31                 \n\t"
            "pop  r30                 \n\t"
            "pop  r27                 \n\t"
            "pop  r26                 \n\t"
            "pop  r25                 \n\t"
            "pop  r24                 \n\t"
            "pop  r23                 \n\t"
            "pop  r22                 \n\t"
            "pop  r21                 \n\t"
            "pop  r20                 \n\t"
            "pop  r19                 \n\t"
            "pop  r18                 \n\t"
            "pop  r1                  \n\t"
            "pop  r0                  \n\t"
            "out  __SREG__, r0        \n\t"
            "pop  r0                  \n\t"
            ::
        );
    }    

    SAVE_CONTEXT();

    if (current_thread_id != 0)
    {
        thread_move_first_to_end();
    }

    current_thread_id = thread_get_first_runnable();
    current_thread = thread_get(current_thread_id);

    if (current_thread->ticks != 0xFF)
    {
        current_thread->ticks = 10;
    }        

    INIT_OR_RESTORE_CONTEXT();
    DISPATCH();
}
