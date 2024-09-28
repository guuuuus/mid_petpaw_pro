#include <neo_ch32vtim1dma.h>
#include <stdlib.h>
volatile unsigned char *_neodataP;
// volatile unsigned long *pointer = _neodataP;
unsigned short _neodataLen;
DMA_InitTypeDef dma;
#define occhan 1
#define baseFreq 400 /// 400 or 800 , set in khz

#if (occhan == 1)
#define ccreg CH1CVR
#define outputPin GPIO_Pin_8
#define ocXinit TIM_OC1Init
#elif (occhan == 2)
#define ccreg CH2CVR
#define outputPin GPIO_Pin_9
#define ocXinit TIM_OC2Init
#elif (occhan == 3)
#define ccreg CH3CVR
#define outputPin GPIO_Pin_10
#define ocXinit TIM_OC3Init
#else
#define ccreg CH4CVR
#define outputPin GPIO_Pin_3
#define ocXinit TIM_OC4Init
#endif

unsigned char _neo_onetime = 0;
unsigned char _neo_zerotime = 0;
unsigned short _neo_latchtime = 0; // for newer ws2812's this could be set to ~5000, but for older ws2811's is now 16000....
unsigned char _neo_period = 0;

void neo_setData(unsigned char *data, unsigned short len, unsigned short startchan)
{

    if ((startchan + len) > _neodataLen)
        return;

    unsigned char *_neodataPixelContent = _neodataP + 1;
    for (unsigned short i = 0; i < len; i++)
    {
        for (unsigned char j = 0; j < 8; j++)
        {
            if ((data[i] & (0x80 >> j)))
                *_neodataPixelContent = _neo_onetime;
            else
                *_neodataPixelContent = _neo_zerotime;
            _neodataPixelContent++;
        }
    }
}

void _neo_calcTimTimes(unsigned long neobasefequency)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    clocks.SYSCLK_Frequency = clocks.SYSCLK_Frequency / 2; // tim prescalers are set to 1, so at 144mhz it is < uncisgned char
    _neo_period = clocks.SYSCLK_Frequency / neobasefequency;

    _neo_onetime = _neo_period / 2;
    _neo_zerotime = _neo_period / 6;

    _neo_latchtime = 0x7fff;

}

void neo_begin(unsigned short numOfchannels)
{

    TIM_TimeBaseInitTypeDef neotim;
    TIM_OCInitTypeDef neooc;
    GPIO_InitTypeDef neogpio;
    NVIC_InitTypeDef NVIC_InitStructure = {0};
    _neodataLen = numOfchannels * 8;
    _neo_calcTimTimes((baseFreq * 1000));

    // last bit is a bit stretched, probably because the itc if fired?
    // just add 1 bit, this will be shifted out bij the last pixel.
    // for some reason the first bit is not send the second af ter itc fires, so append one in front as well
    _neodataP = (unsigned char *)malloc(_neodataLen + 2);
    // set al data to zero
    for (unsigned short i = 0; i < _neodataLen + 2; i++)
    {
        _neodataP[i] = _neo_zerotime;
        // _neodataP[i] = _neo_onetime;
    }

#if defined(CH32X035)
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
#else
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
#endif
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    neogpio.GPIO_Pin = outputPin;
    neogpio.GPIO_Speed = GPIO_Speed_50MHz;
    // neogpio.GPIO_Mode = GPIO_Mode_AF_PP;
    neogpio.GPIO_Mode = GPIO_Mode_AF_OD;

    neotim.TIM_Period = _neo_period;
    neotim.TIM_Prescaler = 0x01;
    neotim.TIM_ClockDivision = TIM_CKD_DIV1;
    neotim.TIM_RepetitionCounter = 0;
    neotim.TIM_CounterMode = TIM_CounterMode_Up;

    neooc.TIM_OCMode = TIM_OCMode_PWM1;
    neooc.TIM_OutputState = TIM_OutputState_Enable;
    neooc.TIM_OutputNState = TIM_OutputNState_Enable;
    neooc.TIM_Pulse = 0xffff;
    neooc.TIM_OCPolarity = TIM_OCPolarity_High;
    neooc.TIM_OCNPolarity = TIM_OCNPolarity_High;
    neooc.TIM_OCIdleState = TIM_OCIdleState_Set;
    neooc.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    dma.DMA_PeripheralBaseAddr = (unsigned long)&TIM1->ccreg;
    dma.DMA_MemoryBaseAddr = (unsigned long *)_neodataP;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = _neodataLen + 2;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    // dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    // dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    // dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_VeryHigh;
    dma.DMA_M2M = DMA_M2M_Disable;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_Init(GPIOA, &neogpio);
    ocXinit(TIM1, &neooc);

    TIM_ARRPreloadConfig(TIM1, ENABLE);

    TIM_TimeBaseInit(TIM1, &neotim);

    DMA_Init(DMA1_Channel5, &dma);
    DMA_Cmd(DMA1_Channel5, ENABLE);

    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);

    TIM_Cmd(TIM1, ENABLE);
    NVIC_Init(&NVIC_InitStructure);
#if defined(CH32X035) || defined(CH32V003)
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;

#else
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
#endif
    NVIC_Init(&NVIC_InitStructure);

    // NVIC_EnableIRQ(TIM2_IRQn);

    TIM_DMACmd(TIM1, TIM_DMA_Update, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

#if defined(CH32X035)
extern void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
#else
extern void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void) // it think using the noneos spl/hal is slower thank read and wirting the register directly?
#endif

{
    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);

    TIM1->ATRLR = _neo_period;
    TIM1->ccreg = _neodataP[0];
    TIM1->CNT = 0;
    TIM_ITConfig(TIM1, TIM_IT_Update, DISABLE);

    DMA_Init(DMA1_Channel5, &dma);
    DMA_Cmd(DMA1_Channel5, ENABLE);

    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);
    TIM_DMACmd(TIM1, TIM_DMA_Update, ENABLE);
    // TIM2->ATRLR = 0x00;
}
void DMA1_Channel5_IRQHandler()
{

    DMA_ClearITPendingBit(DMA1_IT_TC5);
    DMA_Cmd(DMA1_Channel5, DISABLE);

    TIM1->ATRLR = _neo_latchtime;

    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
}