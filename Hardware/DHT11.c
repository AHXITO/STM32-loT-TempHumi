#include "stm32f10x.h"                  // Device header
#include "dht11.h"
#include "Delay.h"

#define DHT11_GPIO  GPIOA
#define DHT11_PIN   GPIO_Pin_1
#define DHT11_RCC   RCC_APB2Periph_GPIOA


static void DHT11_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}

static void DHT11_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}

u8 DHT11_Read_Data(float *temp, u8 *humi)
{
    u8 i, j, data[5] = {0};
    u32 timeout;

    // 1. 启动信号
    DHT11_Output();
    GPIO_ResetBits(DHT11_GPIO, DHT11_PIN);   // 拉低18ms
    Delay_ms(18);
    GPIO_SetBits(DHT11_GPIO, DHT11_PIN);     // 拉高20us
    Delay_us(20);

    // 2. 切换为输入，等待DHT11响应
    DHT11_Input();
    Delay_us(40);

    if (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 0) {  // 低电平响应
        timeout = 10000;
        while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 0 && timeout--);  // 等待80us低
        timeout = 10000;
        while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1 && timeout--);  // 等待80us高

        // 读取40bit数据
        for (i = 0; i < 5; i++) {
            for (j = 0; j < 8; j++) {
                timeout = 10000;
                while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 0 && timeout--);  // 50us低
                Delay_us(30);  // 30us区分0/1
                if (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1) {
                    data[i] |= (1 << (7 - j));
                }
                timeout = 10000;
                while (GPIO_ReadInputDataBit(DHT11_GPIO, DHT11_PIN) == 1 && timeout--);
            }
        }

        // 校验
        if (data[4] == (data[0] + data[1] + data[2] + data[3])) {
            *humi = data[0];
            *temp = (float)data[2] + (float)data[3] * 0.1f;
            return 1;
        }
    }
    return 0;
}
