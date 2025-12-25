#ifndef __APP_SMART_LOCK_H
#define __APP_SMART_LOCK_H
#include "stm32f10x.h"

#define DIST_UNLOCK_THRESHOLD   1200   // 1.2m
#define DIST_LOCK_THRESHOLD     3000   // 3.0m
#define DIST_FAR_AWAY           50000 

void SmartLock_Init(void);
void SmartLock_Run_Logic(uint32_t dist_mm);

#endif