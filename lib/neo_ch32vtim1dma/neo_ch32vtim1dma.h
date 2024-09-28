// 2023 guus. neopixel/ws2811/12 lib using tim1 cc1. (GPIOC, pin 4)
// abuses tim1 pwm to generate long / short pulses. Timing is a little out of spec (300khz instead of 400),
// but seems to work with all the led i have laying around
#if defined(CH32X035)
#include <ch32x035_rcc.h>
#include <ch32x035_gpio.h>
#include <ch32x035_tim.h>
#include <ch32x035_dma.h>
#else
#include <ch32v20x_rcc.h>
#include <ch32v20x_gpio.h>
#include <ch32v20x_tim.h>
#include <ch32v20x_dma.h>

#endif
#ifndef neo_ch32vtim2dma_h
#define neo_ch32vtim2dma_h
// set up timer and gpio
void neo_begin(unsigned short numOfchannels);

void neo_setData(unsigned char *data, unsigned short len, unsigned short startchan);


// extern void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern void DMA1_Channel5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
extern void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#endif