#pragma once

#include "Thread.h"
 
 void thread_entry();
 
 #define INIT_CONTEXT() \
 current_thread->saved_stack_pointer = current_thread->initial_stack_pointer; \
 \
 asm volatile ( \
 "out __SP_H__, %[stack_pointer_high] \n\t" \
 "out __SP_L__, %[stack_pointer_low] \n\t" \
 "movw r30, %[thread_entry] \n\t" \
 "push r30 \n\t" \
 "push r31 \n\t" \
 "clr r0 \n\t" \
 "out __SREG__, r0 \n\t" \
 "clr r31 \n\t" \
 "clr r30 \n\t" \
 "clr r29 \n\t" \
 "clr r28 \n\t" \
 "clr r27 \n\t" \
 "clr r26 \n\t" \
 "clr r25 \n\t" \
 "clr r24 \n\t" \
 "clr r23 \n\t" \
 "clr r22 \n\t" \
 "clr r21 \n\t" \
 "clr r20 \n\t" \
 "clr r19 \n\t" \
 "clr r18 \n\t" \
 "clr r17 \n\t" \
 "clr r16 \n\t" \
 "clr r15 \n\t" \
 "clr r14 \n\t" \
 "clr r13 \n\t" \
 "clr r12 \n\t" \
 "clr r11 \n\t" \
 "clr r10 \n\t" \
 "clr r9 \n\t" \
 "clr r8 \n\t" \
 "clr r7 \n\t" \
 "clr r6 \n\t" \
 "clr r5 \n\t" \
 "clr r4 \n\t" \
 "clr r3 \n\t" \
 "clr r2 \n\t" \
 "clr r1 \n\t" \
 "clr r0 \n\t" \
 : \
 : [thread_entry] "r" (thread_entry), \
 [stack_pointer_high] "r" ((uint8_t)(current_thread->saved_stack_pointer >> 8)), \
 [stack_pointer_low] "r" ((uint8_t)(current_thread->saved_stack_pointer & 0xFF)) \
 : "memory" \
 )

 #define SAVE_CONTEXT_R16_ASM \
 "push r16                  \n\t" \

 #define SAVE_CONTEXT_R16() \
 asm volatile ( \
 SAVE_CONTEXT_R16_ASM \
 ::: \
 )

 #define SAVE_CONTEXT_R16_SREG_ASM \
 "push r16                  \n\t" \
 "in   r16, __SREG__        \n\t" \
 "push r16                  \n\t"

 #define SAVE_CONTEXT_R16_SREG() \
 asm volatile ( \
 SAVE_CONTEXT_R16_SREG_ASM \
 ::: \
 )

 #define SAVE_CONTEXT_R16_Z_SREG_ASM \
 "push r16                  \n\t" \
 "in   r16, __SREG__        \n\t" \
 "push r16                  \n\t" \
 "push r30                  \n\t" \
 "push r31                  \n\t"

 #define SAVE_CONTEXT_R16_Z_SREG() \
 asm volatile ( \
 SAVE_CONTEXT_R16_Z_SREG_ASM \
 ::: \
 )

 #define SAVE_CONTEXT() \
 asm volatile ( \
 "push r0                  \n\t" /* Save R0 (used as scratchpad) */ \
 "in   r0, __SREG__        \n\t" /* Copy SREG into R0 (0x3F) */ \
 "push r0                  \n\t" /* Push SREG onto the stack */ \
 "push r1                  \n\t" /* Save R1 */ \
 "clr  r1                  \n\t" /* Clear R1 (C expects R1 to be 0) */ \
 "push r2                  \n\t" \
 "push r3                  \n\t" \
 "push r4                  \n\t" \
 "push r5                  \n\t" \
 "push r6                  \n\t" \
 "push r7                  \n\t" \
 "push r8                  \n\t" \
 "push r9                  \n\t" \
 "push r10                 \n\t" \
 "push r11                 \n\t" \
 "push r12                 \n\t" \
 "push r13                 \n\t" \
 "push r14                 \n\t" \
 "push r15                 \n\t" \
 "push r16                 \n\t" \
 "push r17                 \n\t" \
 "push r18                 \n\t" \
 "push r19                 \n\t" \
 "push r20                 \n\t" \
 "push r21                 \n\t" \
 "push r22                 \n\t" \
 "push r23                 \n\t" \
 "push r24                 \n\t" \
 "push r25                 \n\t" \
 "push r26                 \n\t" /* X pointer low */ \
 "push r27                 \n\t" /* X pointer high */ \
 "push r28                 \n\t" /* Y pointer low (Frame pointer) */ \
 "push r29                 \n\t" /* Y pointer high */ \
 "push r30                 \n\t" /* Z pointer low */ \
 "push r31                 \n\t" /* Z pointer high */ \
 : \
 : \
 : "memory" \
 ); \
 \
 asm volatile ( \
 "in %A[stack_pointer], __SP_L__ \n\t" \
 "in %B[stack_pointer], __SP_H__ \n\t" \
 : [stack_pointer] "=r" (current_thread->saved_stack_pointer) \
 )

 #define RESTORE_CONTEXT() \
 asm volatile ( \
 "out __SP_H__, %[stack_pointer_high] \n\t" \
 "out __SP_L__, %[stack_pointer_low] \n\t" \
 "pop  r31                 \n\t" /* Restore Z pointer high byte */ \
 "pop  r30                 \n\t" /* Restore Z pointer low byte */ \
 "pop  r29                 \n\t" /* Restore Y pointer high byte */ \
 "pop  r28                 \n\t" /* Restore Y pointer low byte (Frame Pointer) */ \
 "pop  r27                 \n\t" /* Restore X pointer high byte */ \
 "pop  r26                 \n\t" /* Restore X pointer low byte */ \
 "pop  r25                 \n\t" \
 "pop  r24                 \n\t" \
 "pop  r23                 \n\t" \
 "pop  r22                 \n\t" \
 "pop  r21                 \n\t" \
 "pop  r20                 \n\t" \
 "pop  r19                 \n\t" \
 "pop  r18                 \n\t" \
 "pop  r17                 \n\t" \
 "pop  r16                 \n\t" \
 "pop  r15                 \n\t" \
 "pop  r14                 \n\t" \
 "pop  r13                 \n\t" \
 "pop  r12                 \n\t" \
 "pop  r11                 \n\t" \
 "pop  r10                 \n\t" \
 "pop  r9                  \n\t" \
 "pop  r8                  \n\t" \
 "pop  r7                  \n\t" \
 "pop  r6                  \n\t" \
 "pop  r5                  \n\t" \
 "pop  r4                  \n\t" \
 "pop  r3                  \n\t" \
 "pop  r2                  \n\t" \
 "pop  r1                  \n\t" /* Restore R1 */ \
 "pop  r0                  \n\t" /* Pop the saved SREG value into R0 temporary space */ \
 "out  __SREG__, r0        \n\t" /* Write the value back into the actual SREG register (0x3F) */ \
 "pop  r0                  \n\t" /* Restore the true original R0 register value */ \
 : /* No outputs */ \
 : [thread_entry] "r" (thread_entry), \
 [stack_pointer_high] "r" ((uint8_t)(current_thread->saved_stack_pointer >> 8)), \
 [stack_pointer_low] "r" ((uint8_t)(current_thread->saved_stack_pointer & 0xFF)) \
 : "memory" /* Prevent the compiler from reordering reads/writes across this macro */ \
 )

 #define RESTORE_CONTEXT_R16_ASM \
 "pop  r16                 \n\t" \

 #define RESTORE_CONTEXT_R16() \
 asm volatile ( \
 RESTORE_CONTEXT_R16_ASM \
 ::: \
 );

 #define RESTORE_CONTEXT_R16_SREG_ASM \
 "pop  r16                 \n\t" \
 "out  __SREG__, r16        \n\t" \
 "pop  r16                  \n\t"

 #define RESTORE_CONTEXT_R16_SREG() \
 asm volatile ( \
 RESTORE_CONTEXT_R16_SREG_ASM \
 ::: \
 );

 #define RESTORE_CONTEXT_R16_Z_SREG_ASM \
 "pop  r31                 \n\t" \
 "pop  r30                 \n\t" \
 "pop  r16                 \n\t" \
 "out  __SREG__, r16        \n\t" \
 "pop  r16                  \n\t"

 #define RESTORE_CONTEXT_R16_Z_SREG() \
 asm volatile ( \
 RESTORE_CONTEXT_R16_Z_SREG \
 ::: \
 );

 #define INIT_OR_RESTORE_CONTEXT() if (current_thread->saved_stack_pointer == 0xFFFF) { INIT_CONTEXT(); } else { RESTORE_CONTEXT(); }

 #define DISPATCH() \
 asm volatile ( \
 "push r16 \n\t" \
 "ldi r16, 0 \n\t" \
 "sts interrupt_level, r16 \n\t" \
 "pop r16 \n\t" \
 "reti \n\t" \
 ::: \
 )
