#include "hal_gps.h"
#include <string.h>
#include <stdio.h>

// ????,??????
Lock_GPS_Data_t g_GPS_CurrentData = { "0", "0", 0 };

#define GPS_BUF_SIZE 128
volatile char gps_rx_buffer[GPS_BUF_SIZE];
volatile uint8_t gps_rx_index = 0;
volatile uint8_t gps_frame_received = 0;

void GPS_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // PA2 TX, PA3 RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // ??? 9600
    USART_InitStructure.USART_BaudRate = 9600; 
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    // ??????
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);

    // ???????
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

// ?? USART2 ??????
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        char ch = USART_ReceiveData(USART2);
        // ?????: ??????
        if (ch == '\n') {
            gps_rx_buffer[gps_rx_index] = 0; // ??????
            gps_frame_received = 1;          // ?????
            gps_rx_index = 0;                // ????
        } else {
            if (gps_rx_index < GPS_BUF_SIZE - 1) {
                gps_rx_buffer[gps_rx_index++] = ch;
            }
        }
    }
}

// ????: ?????????
void Get_Comma_Field(char* src, int num, char* dest) {
    int comma = 0, i = 0, j = 0;
    // ?? num ???
    while (comma < num) {
        if (src[i] == 0) return;
        if (src[i++] == ',') comma++;
    }
    // ???????????
    while (src[i] != ',' && src[i] != '*' && src[i] != 0 && j < 15) {
        dest[j++] = src[i++];
    }
    dest[j] = 0;
}

// ???? (???????)
void GPS_Parse_Buffer(void)
{
    if (gps_frame_received) {
        // ?? $GNRMC (??) ? $GPRMC
        if (strstr((char*)gps_rx_buffer, "RMC")) {
            char valid[2];
            // ??2: ?? (A=??)
            Get_Comma_Field((char*)gps_rx_buffer, 2, valid);
            
            if (valid[0] == 'A') {
                g_GPS_CurrentData.is_valid = 1;
                // ??3: ??, ??5: ??
                Get_Comma_Field((char*)gps_rx_buffer, 3, g_GPS_CurrentData.latitude);
                Get_Comma_Field((char*)gps_rx_buffer, 5, g_GPS_CurrentData.longitude);
            } else {
                g_GPS_CurrentData.is_valid = 0;
            }
        }
        gps_frame_received = 0; // ????
    }
}