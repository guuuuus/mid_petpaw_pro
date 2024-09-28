#ifndef millis_h
#define millis_h

#if defined(CH32X035)
#include <ch32x035.h>
#endif
#if defined(CH32V10X)
#include <ch32v10x.h>
#endif
#if defined(CH32V20X)
#include <ch32v20x.h>
#endif
#if defined(CH32V30X)
#include <ch32v30x.h>
#endif

#if defined(CH32V00X)
#include <ch32v00x.h>
volatile unsigned long millis_cntOverflow = 0;
#endif

unsigned long millis_ticksInMs;
unsigned long millis_ticksInUs;
unsigned long long millis()
{
#if defined(CH32V00X)
    unsigned long long t = (millis_cntOverflow * 0x100000000) / millis_ticksInMs;
    return (t + (SysTick->CNT / millis_ticksInMs));
#else
    return (SysTick->CNT / millis_ticksInMs);
#endif
}

unsigned long long micros()
{
#if defined(CH32V00X)
    unsigned long long t = (millis_cntOverflow * 0x100000000) / millis_ticksInUs;
    return (t + (SysTick->CNT / millis_ticksInUs));
#else
    return (SysTick->CNT / millis_ticksInUs);
#endif
}

void millis_init()
{
    millis_ticksInMs = SystemCoreClock / 8000;
    millis_ticksInUs = SystemCoreClock / 8000000;

#if defined(CH32V00X)
    // enable swi
    SysTick->CTLR |= 0x80000000;
    // autoreload to count up
    SysTick->CTLR &= ~(0x00000010);
    // autoreload to continue
    SysTick->CTLR &= ~(0x00000008);
    // hclk /8
    SysTick->CTLR &= ~(0x00000004);
    // enable interupt
    SysTick->CTLR |= 0x00000002;
    // SysTick->CTLR &= ~(0x00000002);
    // set counter to 0
    SysTick->CNT = 0x00000000;
    // set compare
    SysTick->CMP = 0xffffffff;
    NVIC_EnableIRQ(SysTicK_IRQn);
    // start systick
    SysTick->CTLR |= 0x00000001;

    millis_cntOverflow = 0;
#else
    // disable swi
    SysTick->CTLR &= ~(0x80000000);
    // autoreload to count up
    SysTick->CTLR &= ~(0x00000010);
    // autoreload to continue
    SysTick->CTLR &= ~(0x00000008);
    // hclk /8
    SysTick->CTLR &= ~(0x00000004);
    // disable interupt
    SysTick->CTLR &= ~(0x00000002);
    // set counter to 0
    SysTick->CNT = 0;
    // start systick
    SysTick->CTLR |= 0x00000001;
#endif
}

void delay(unsigned long t)
{
    unsigned long current = millis();
    while (millis() - current < t)
        ;
}

void delay_micros(unsigned long t)
{
    unsigned long current = micros();
    while (micros() - current < t)
        ;
}

#if defined(CH32V00X)
extern void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    // reset counter
    SysTick->CNT = 0;
    // clear swi
    SysTick->CTLR &= ~(0x80000000);
    //?clear cmpare?
    SysTick->SR &= ~(0x00000001);
    // reset counter
    SysTick->CNT = 0;
    // increment overflow cnt
    millis_cntOverflow++;
}
#endif
#endif