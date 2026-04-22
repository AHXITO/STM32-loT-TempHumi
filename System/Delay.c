#include "stm32f10x.h"

volatile uint32_t system_tick = 0;


void Delay_us(uint32_t xus)
{
    uint32_t ticks = xus * (SystemCoreClock / 1000000);
    uint32_t told = SysTick->VAL;
    uint32_t tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;

    while (1) {
        tnow = SysTick->VAL;
        if (tnow != told) {
            if (tnow < told) tcnt += told - tnow;
            else tcnt += reload - tnow + told;
            told = tnow;
            if (tcnt >= ticks) break;
        }
    }
}

void Delay_ms(uint32_t xms)
{
    while (xms--) {
        Delay_us(1000);
    }
}

void Delay_s(uint32_t xs)
{
    while (xs--) {
        Delay_ms(1000);
    }
}

/*SysTick 初始化和计时 */

void Delay_Init(void)
{
    system_tick = 0;
    SysTick_Config(SystemCoreClock / 1000);   // 1ms 中断一次
}

uint32_t Get_SystemTick(void)
{
    return system_tick;
}
