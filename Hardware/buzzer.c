#include "stm32f10x.h"                  // Device header
#include "buzzer.h"


void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_GPIO, &GPIO_InitStructure);
    Buzzer_Off();
}

void Buzzer_On(void)
{
    GPIO_ResetBits(BUZZER_GPIO, BUZZER_PIN);
}

void Buzzer_Off(void)
{
    GPIO_SetBits(BUZZER_GPIO, BUZZER_PIN);
}
