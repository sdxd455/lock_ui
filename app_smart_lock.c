#include "app_smart_lock.h"
#include "hal_servo.h"
#include "hal_gps.h"     
#include "stm32f10x.h"   
#include <stdio.h>
#include <string.h>

extern volatile uint32_t g_system_time_ms;

// ????
static void Delay_Ms_Blocking(uint32_t ms) {
    uint32_t i, j;
    // ?? 64MHz ??????
    for(i=0; i<ms; i++) for(j=0; j<6000; j++) __NOP();
}

void BT_SendString(char* str) {
    while (*str) {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
        USART_SendData(USART3, *str++);
    }
}

typedef enum { STATUS_LOCKED = 0, STATUS_UNLOCKED = 1 } SysLockStatus_e;
static SysLockStatus_e g_CurrentStatus = STATUS_LOCKED;

#define FILTER_DEPTH 20        
static uint32_t dist_buffer[FILTER_DEPTH] = {0};
static uint8_t  buf_idx = 0;
static uint8_t  buf_filled = 0; 
#define LOST_SIGNAL_MAX_COUNT 6
static uint8_t lost_signal_count = 0;
static uint32_t last_valid_dist_mm = 5000;

void SmartLock_Init(void) {
    Servo_Init();
    Servo_Control(SERVO_STOP);
    printf("[SmartLock] Ready (Duration 2.8s).\r\n");
}

static void Report_RealTime_Status(uint32_t dist_mm) {
    char buffer[128];
    char gps_str[64];
    char lock_str[10];

    if (g_GPS_CurrentData.is_valid) 
        sprintf(gps_str, "LAT:%s,LON:%s", g_GPS_CurrentData.latitude, g_GPS_CurrentData.longitude);
    else 
        sprintf(gps_str, "GPS:Searching...");

    if (g_CurrentStatus == STATUS_LOCKED) sprintf(lock_str, "LOCKED");
    else sprintf(lock_str, "OPEN");

    if (dist_mm > 0) sprintf(buffer, "DIST:%dmm | %s | %s\r\n", dist_mm, gps_str, lock_str);
    else sprintf(buffer, "DIST:Analyzing... | %s | %s\r\n", gps_str, lock_str);

    printf("%s", buffer);    
    BT_SendString(buffer);   
}

uint32_t Get_Stable_Distance(uint32_t new_dist) {
    dist_buffer[buf_idx++] = new_dist;
    if (buf_idx >= FILTER_DEPTH) { buf_idx = 0; buf_filled = 1; }
    if (buf_filled == 0) return new_dist;

    uint32_t sum = 0, max1 = 0, max2 = 0, min1 = -1, min2 = -1;
    for (int i = 0; i < FILTER_DEPTH; i++) {
        uint32_t val = dist_buffer[i];
        sum += val;
        if (val > max1) { max2 = max1; max1 = val; } else if (val > max2) max2 = val;
        if (val < min1) { min2 = min1; min1 = val; } else if (val < min2) min2 = val;
    }
    return (sum - max1 - max2 - min1 - min2) / (FILTER_DEPTH - 4);
}

// ??? ????:?????? ???
void Execute_Servo_Action(uint8_t action) {
    // ?????? (??)
    // ??? 1400,????? 2800 (2.8?)
    // ?????,??????? (?? 4000)
    uint32_t rotate_time = 4000;

    if (action == SERVO_OPEN) {
        printf("[Servo] Rotating OPEN (%dms)...\r\n", rotate_time);
        Servo_Control(SERVO_OPEN); 
        Delay_Ms_Blocking(rotate_time);   
        Servo_Control(SERVO_STOP); 
    } 
    else if (action == SERVO_CLOSE) {
        printf("[Servo] Rotating CLOSE (%dms)...\r\n", rotate_time);
        Servo_Control(SERVO_CLOSE); 
        Delay_Ms_Blocking(rotate_time);    
        Servo_Control(SERVO_STOP);  
    }
}

void SmartLock_Run_Logic(uint32_t raw_dist_mm) {
    static uint32_t last_report_time = 0;
    uint32_t final_dist = 0;

    if (raw_dist_mm == 0) {
        if ((g_CurrentStatus == STATUS_UNLOCKED) && (last_valid_dist_mm < 800)) {
            lost_signal_count = 0;
            final_dist = last_valid_dist_mm; 
        } else {
            lost_signal_count++;
            if (lost_signal_count >= LOST_SIGNAL_MAX_COUNT) {
                if (g_CurrentStatus == STATUS_UNLOCKED) {
                    printf("[Action] Lost -> LOCKING\r\n");
                    Execute_Servo_Action(SERVO_CLOSE);
                    g_CurrentStatus = STATUS_LOCKED;
                }
                lost_signal_count = LOST_SIGNAL_MAX_COUNT;
            }
            final_dist = 0;
        }
    } else {
        lost_signal_count = 0;
        final_dist = Get_Stable_Distance(raw_dist_mm);
        last_valid_dist_mm = final_dist;
    }

    if (final_dist > 0) {
        if ((final_dist <= DIST_UNLOCK_THRESHOLD) && (g_CurrentStatus == STATUS_LOCKED)) {
            printf("[Action] Near (%d) -> UNLOCKING\r\n", final_dist);
            Execute_Servo_Action(SERVO_OPEN);
            g_CurrentStatus = STATUS_UNLOCKED;
        }
        else if ((final_dist > DIST_LOCK_THRESHOLD) && (g_CurrentStatus == STATUS_UNLOCKED)) {
            printf("[Action] Left (%d) -> LOCKING\r\n", final_dist);
            Execute_Servo_Action(SERVO_CLOSE);
            g_CurrentStatus = STATUS_LOCKED;
        }
    }

    if ((g_system_time_ms - last_report_time) >= 1000) {
        last_report_time = g_system_time_ms;
        Report_RealTime_Status(final_dist);
    }
}