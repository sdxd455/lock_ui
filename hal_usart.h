#ifndef __HAL_USART_H
#define __HAL_USART_H
#include "stm32f10x.h"
#include <stdio.h>

void HalUSART1_Init(void);
void HalUSART3_Init(void);
int fputc(int ch, FILE *f);

// ????
extern volatile char USART1_DMA_RX_BYTE;
uint16_t USART1_SendBuffer(const char* buffer, uint16_t length, int flag);

#endif