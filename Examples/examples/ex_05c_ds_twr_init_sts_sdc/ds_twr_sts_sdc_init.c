/* A ? (Initiator) - ??????? */
#include <deca_device_api.h>
#include <deca_regs.h>
#include <shared_defines.h>
#include <shared_functions.h>
#include <example_selection.h>
#include "uwb.h"
#include "app_smart_lock.h" 
#include "hal_gps.h"        
#include "stm32f10x.h"
#include <stdio.h>

extern volatile uint32_t g_system_time_ms;
extern dwt_txconfig_t txconfig_options;

void Toggle_LED_Now(void) {
    uint8_t bit = GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1);
    GPIO_WriteBit(GPIOA, GPIO_Pin_1, (bit ? Bit_RESET : Bit_SET));
}
void Delay_Safe(uint32_t count) { while(count--); }
void Sleep_With_GPS(uint32_t ms) {
    uint32_t i;
    for(i = 0; i < ms / 10; i++) { GPS_Parse_Buffer(); Sleep(10); }
}

#define SPEED_OF_LIGHT      299792458.0
#define DWT_TIME_UNITS      (1.0/499.2e6/128.0)

// ??? ????????????? 524641xxx ???? ???
// ???,??????? 1? ~ 5? ??
#define RESPONDER_DELAY_EST (319555600) 

#define APP_NAME "Smart Lock"
#define RNG_DELAY_MS 500 

// ??????
static dwt_config_t config = {
    5, DWT_PLEN_128, DWT_PAC8, 9, 9, 1, 
    DWT_BR_850K, DWT_PHRMODE_STD, DWT_PHRRATE_STD, (129 + 8 - 8), 
    DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};

#define TX_ANT_DLY 0
#define RX_ANT_DLY 0

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21};
static uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x02, 0, 0};
static uint8_t tx_final_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define FINAL_MSG_POLL_TX_TS_IDX 10
#define FINAL_MSG_RESP_RX_TS_IDX 14
#define FINAL_MSG_FINAL_TX_TS_IDX 18
static uint8_t frame_seq_nb = 0;
#define RX_BUF_LEN 24
static uint8_t rx_buffer[RX_BUF_LEN];
static uint32_t status_reg = 0;

// ??????
#define POLL_TX_TO_RESP_RX_DLY_UUS 0 
#define RESP_RX_TO_FINAL_TX_DLY_UUS 3000 
#define RESP_RX_TIMEOUT_UUS 20000 

#define PRE_TIMEOUT 5
static uint64_t poll_tx_ts;
static uint64_t resp_rx_ts;
static uint64_t final_tx_ts;

int ds_twr_sts_sdc_initiator(void)
{
    int retry = 0;
    port_set_dw_ic_spi_slowrate();
    reset_DWIC(); 
    Sleep(10); 
    while (!dwt_checkidlerc()) { retry++; Sleep(1); if(retry > 200) break; };
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) { while (1) { Toggle_LED_Now(); Delay_Safe(100000); }; }
    port_set_dw_ic_spi_fastrate();
    if(dwt_configure(&config)) { while (1) { Toggle_LED_Now(); Delay_Safe(100000); }; }
    
    dwt_setleds(0x03); 
    dwt_configuretxrf(&txconfig_options);
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);
    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);
    dwt_setpreambledetecttimeout(0); 
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    printf("Calibration applied.\r\n");

    while (1)
    {
        GPS_Parse_Buffer();

        tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
        dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0); 
        dwt_writetxfctrl(sizeof(tx_poll_msg)+FCS_LEN, 0, 1); 
        dwt_write32bitreg(SYS_STATUS_ID, 0xFFFFFFFF);
        dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

        while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) { };
        frame_seq_nb++;

        if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) 
        {
            Toggle_LED_Now(); 
            uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
            if (frame_len <= RX_BUF_LEN) dwt_readrxdata(rx_buffer, frame_len, 0);
            rx_buffer[ALL_MSG_SN_IDX] = 0;
            
            if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) 
            {
                uint32_t final_tx_time;
                int ret;
                poll_tx_ts = get_tx_timestamp_u64();
                resp_rx_ts = get_rx_timestamp_u64();

                int64_t rtt = resp_rx_ts - poll_tx_ts;
                
                // ????
                double tof = (rtt - RESPONDER_DELAY_EST) * 0.5 * DWT_TIME_UNITS;
                double dist_m = tof * SPEED_OF_LIGHT;
                uint32_t dist_mm = (uint32_t)(dist_m * 1000);
                
                if(dist_m < 0) dist_mm = 0; 
                
                // ???????
                SmartLock_Run_Logic(dist_mm);

                final_tx_time = (resp_rx_ts + (RESP_RX_TO_FINAL_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
                dwt_setdelayedtrxtime(final_tx_time);
                final_tx_ts = (((uint64_t)(final_tx_time & 0xFFFFFFFEUL)) << 8) + TX_ANT_DLY;
                final_msg_set_ts(&tx_final_msg[FINAL_MSG_POLL_TX_TS_IDX], poll_tx_ts);
                final_msg_set_ts(&tx_final_msg[FINAL_MSG_RESP_RX_TS_IDX], resp_rx_ts);
                final_msg_set_ts(&tx_final_msg[FINAL_MSG_FINAL_TX_TS_IDX], final_tx_ts);
                tx_final_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
                dwt_writetxdata(sizeof(tx_final_msg), tx_final_msg, 0); 
                dwt_writetxfctrl(sizeof(tx_final_msg)+FCS_LEN, 0, 1); 
                ret = dwt_starttx(DWT_START_TX_DELAYED);
                if (ret == DWT_SUCCESS) {
                    while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS_BIT_MASK)) { };
                    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
                    frame_seq_nb++;
                }
            }
        }
        else {
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
            SmartLock_Run_Logic(0); 
        }
        Sleep_With_GPS(RNG_DELAY_MS);
    }
}