#include <debug.h>
#include <millis.h>
#include <neo_ch32vtim1dma.h>
#include <usb_midi.h>
// #include <usb_serial.h>
#include <ch32v20x.h>

#include <stdlib.h>

// 8bit touch return, from timer loop
volatile unsigned char touchbutton[6];

unsigned char neopix[6];

#define touchtreshhold 3000
#define touchmax 1500

#define noteoffset 60

void Touch_Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
    TKey1->CTLR1 |= (1 << 26) | (1 << 24); // Enable TouchKey and Buffer
}

u16 Touch_Key_Adc(u8 ch)
{
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_7Cycles5);
    TKey1->IDATAR1 = 0x14; // Charging Time
    TKey1->RDATAR = 0x08;  // Discharging Time
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
        ;
    return (uint16_t)TKey1->RDATAR;
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

extern void TIM4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM4_IRQHandler(void) // it think using the noneos spl/hal is slower thank read and wirting the register directly?
{
    TIM_ClearITPendingBit(TIM4, 0xffff);
    for (unsigned char i = 0; i < 6; i++)
    {
        unsigned short t = Touch_Key_Adc(i);
        if (t > touchtreshhold)
            touchbutton[i] = 0;
        else if (t < touchmax)
            touchbutton[i] = 255;
        else
            touchbutton[i] = map(t, touchtreshhold, touchmax, 0, 255);
    }
}

void begintim()
{
    TIM_TimeBaseInitTypeDef tim;
    tim.TIM_ClockDivision = 64;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 22500; // 100/ps
    tim.TIM_RepetitionCounter = 0;

    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = TIM4_IRQn;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseInit(TIM4, &tim);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    NVIC_Init(&nvic);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
}

void toggleloop()
{
    unsigned char state[6] = {0, 0, 0, 0, 0, 0};
    unsigned char lastd[6] = {0, 0, 0, 0, 0, 0};
    unsigned char pix[6] = {0, 0, 0, 0, 0, 0};

    while (1)
    {
        unsigned char newd[6];
        for (unsigned char i = 0; i < 6; i++)
        {
            (newd[i] = touchbutton[i] >> 1);
            switch (state[i])
            {
            case 0: // from off to on
            {
                if (newd[i])
                {
                    state[i] = 1;
                    usbMidi_sendNoteOn(noteoffset + i, newd[i], 0);
                    lastd[i] = newd[i];
                    pix[i] = (newd[i] << 1);
                    lastd[i] = newd[i];
                }
            }
            break;

            case 1: // from on to touch rel or touch change
            {
                if (newd[i] == 0)
                {
                    state[i] = 2;
                }
                else
                {
                    if (newd[i] > lastd[i])
                    {
                        usbMidi_sendNoteOn(noteoffset + i, newd[i], 0);
                        pix[i] = (newd[i] << 1);
                        lastd[i] = newd[i];
                    }
                }
            }
            break;

            case 2:
            {
                if (newd[i])
                {
                    usbMidi_sendNoteOff(noteoffset + i, 0x7f, 0);
                    state[i] = 3;
                    pix[i] = 0;
                }
            }
            break;

            case 3:
                if (newd[i] == 0)
                {
                    state[i] = 0;
                    pix[i] = 0;
                }

            default:
                break;
            }
        }

        neo_setData(pix, 6, 0);
        delay(10);
    }
}

void pressloop()
{
    unsigned char lastd[6] = {0, 0, 0, 0, 0, 0};

    while (1)
    {
        unsigned char newd[6];
        for (unsigned char i = 0; i < 6; i++)
        {
            (newd[i] = touchbutton[i] >> 1);
            if (newd[i] != lastd[i])
            {
                if (newd[i] == 0)
                {

                    usbMidi_sendNoteOff(i + noteoffset, 0, 0);
                    lastd[i] = newd[i];
                }
                else
                {
                    if ((lastd[i] == 0) && (newd[i] > 0x05))
                    {
                        usbMidi_sendNoteOn((rand() & 0x07f), newd[i], 0);
                        // else
                        // // ;
                        //     usbMidi_sendPoly(i + noteoffset, newd[i], 0);
                        lastd[i] = newd[i];
                    }
                }
            }
        }

        neo_setData(lastd, 6, 0);
    }
}

int main()
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    millis_init();
    neo_begin(6);
    usbMidi_begin();
    // usbSerial_begin();
    unsigned char neopix[6] = {0, 0, 0, 0, 0, 0};

    neo_setData(neopix, 6, 0);
    Touch_Key_Init();
    begintim();
    delay(20);
    if (touchbutton[3] && touchbutton[4])
        pressloop();
    else
        toggleloop();
}