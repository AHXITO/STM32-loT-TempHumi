#ifndef __USART2_H
#define __USART2_H

#include "stm32f10x.h"

#define USART2_BUF_SIZE 512

extern char esp_rx_buffer[USART2_BUF_SIZE];
extern volatile uint16_t esp_rx_index;

void USART2_Init(uint32_t BaudRate);
void USART2_SendChar(char ch);
void USART2_SendString(char *str);

uint8_t ESP_FindString(char *target);
void ESP_ClearBuffer(void);

#endif
