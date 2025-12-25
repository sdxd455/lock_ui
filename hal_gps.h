#ifndef __HAL_GPS_H
#define __HAL_GPS_H
#include "stm32f10x.h"

// ??????
typedef struct {
    char latitude[16];  
    char longitude[16]; 
    uint8_t is_valid;   
} Lock_GPS_Data_t;

extern Lock_GPS_Data_t g_GPS_CurrentData; 

void GPS_Init(void);      
void GPS_Parse_Buffer(void); 

#endif