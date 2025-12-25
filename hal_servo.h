#ifndef __HAL_SERVO_H
#define __HAL_SERVO_H
#include "stm32f10x.h"

// 360???????
#define SERVO_STOP   0  // ??
#define SERVO_OPEN   1  // ?? (??)
#define SERVO_CLOSE  2  // ?? (??)

void Servo_Init(void);
// state: 0=?, 1=??, 2=??
void Servo_Control(uint8_t action);

#endif