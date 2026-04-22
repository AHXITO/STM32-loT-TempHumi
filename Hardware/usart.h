#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>

#define USART1_BUF_SIZE 512

extern char usart1_rx_buffer[USART1_BUF_SIZE];
extern volatile uint16_t usart1_rx_index;

void USART1_Init(uint32_t BaudRate);
void USART1_SendChar(char ch);
void USART1_SendString(char *str);
void USART1_ClearBuffer(void);

#endif
