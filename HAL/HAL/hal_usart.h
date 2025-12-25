#ifndef __HAL_USART_H
#define __HAL_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"
#include <stdio.h>

// ============================================================
// ???? (API)
// ============================================================

// ??? ????:????????,?? void ???
void HalUSART1_Init(void); 

// ???????
void HalUSART3_Init(void);

// ??? printf
int fputc(int ch, FILE *f);

// ???????
extern volatile char USART1_DMA_RX_BYTE;
uint16_t USART1_SendBuffer(const char* buffer, uint16_t length, int flag);

#ifdef __cplusplus
}
#endif

#endif  // __HAL_USART_H