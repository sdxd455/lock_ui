#include "hal_servo.h"

// ??? (?????,?????)
#define SERVO_STOP_VAL  1500 

void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // ?? 64MHz ??: 64M / (63+1) = 1MHz
    TIM_TimeBaseStructure.TIM_Prescaler = 63; 
    
    TIM_TimeBaseStructure.TIM_Period = 19999; 
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = SERVO_STOP_VAL; 

    TIM_OC3Init(TIM4, &TIM_OCInitStructure); 
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_Cmd(TIM4, ENABLE);
}

void Servo_Control(uint8_t action)
{
    if (action == SERVO_OPEN) {
        // ??? ??:???? ???
        // ??? 2000,?? 1000
        TIM_SetCompare3(TIM4, 1000); 
    } 
    else if (action == SERVO_CLOSE) {
        // ??? ??:???? ???
        // ??? 1000,?? 2000
        TIM_SetCompare3(TIM4, 2000); 
    } 
    else {
        TIM_SetCompare3(TIM4, SERVO_STOP_VAL); // ??
    }
}