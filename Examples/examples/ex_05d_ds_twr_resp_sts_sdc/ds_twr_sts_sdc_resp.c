/* B ? (Responder) - ????? */
#include <deca_device_api.h>
#include <deca_regs.h>
#include <shared_defines.h>
#include <shared_functions.h>
#include "stm32f10x.h"
#include <stdio.h>

extern volatile uint32_t g_system_time_ms;
extern dwt_txconfig_t txconfig_options;

void Toggle_LED_Now(void) {
    uint8_t bit = GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1);
    GPIO_WriteBit(GPIOA, GPIO_Pin_1, (bit ? Bit_RESET : Bit_SET));
}
void Delay_Safe(uint32_t count) { while(count--); }

// ??????: Ch5, 850K
static dwt_config_t config = {
    5, DWT_PLEN_128, DWT_PAC8, 9, 9, 1, 
    DWT_BR_850K, DWT_PHRMODE_STD, DWT_PHRRATE_STD, (129 + 8 - 8), 
    DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};

#define TX_ANT_DLY 0
#define RX_ANT_DLY 0

static uint8_t rx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x21};
static uint8_t tx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0x10, 0x02, 0, 0};
static uint8_t rx_final_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2

// ??? ????:????? 5000us (5ms) ???
// ?? B ???????????,???? Reply Late
#define POLL_RX_TO_RESP_TX_DLY_UUS 5000 
#define RESP_TX_TO_FINAL_RX_DLY_UUS 500
#define PRE_TIMEOUT 5

static uint8_t frame_seq_nb = 0;
#define RX_BUF_LEN 24
static uint8_t rx_buffer[RX_BUF_LEN];
static uint32_t status_reg = 0;
static uint64_t poll_rx_ts;
static uint64_t resp_tx_ts;

int ds_twr_sts_sdc_responder(void)
{
    static uint32_t last_blink = 0;
    int retry = 0;

    port_set_dw_ic_spi_slowrate();
    reset_DWIC(); 
    Sleep(10); 
    while (!dwt_checkidlerc()) { 
        retry++; Sleep(1);
        if(retry > 200) break; 
    };
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) { while (1) { Toggle_LED_Now(); Delay_Safe(100000); }; }
    port_set_dw_ic_spi_fastrate();
    if(dwt_configure(&config)) { while (1) { Toggle_LED_Now(); Delay_Safe(100000); }; }
    
    dwt_setleds(0x03); 
    dwt_configuretxrf(&txconfig_options);
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);
    dwt_setpreambledetecttimeout(PRE_TIMEOUT);
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    while (1)
    {
        if ((g_system_time_ms - last_blink) > 1000) { Toggle_LED_Now(); last_blink = g_system_time_ms; }
        dwt_write32bitreg(SYS_STATUS_ID, 0xFFFFFFFF);
        dwt_rxenable(DWT_START_RX_IMMEDIATE);

        while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) {
            if ((g_system_time_ms - last_blink) > 1000) { Toggle_LED_Now(); last_blink = g_system_time_ms; }
        };

        if (status_reg & SYS_STATUS_RXFCG_BIT_MASK)
        {
            uint32_t frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
            if (frame_len <= RX_BUF_LEN) dwt_readrxdata(rx_buffer, frame_len, 0);
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

            // ?????
            {
                Toggle_LED_Now(); 
                uint32_t resp_tx_time;
                poll_rx_ts = get_rx_timestamp_u64();
                
                // ?? 5000us ?????
                resp_tx_time = (poll_rx_ts + (POLL_RX_TO_RESP_TX_DLY_UUS * UUS_TO_DWT_TIME)) >> 8;
                dwt_setdelayedtrxtime(resp_tx_time);

                tx_resp_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
                dwt_writetxdata(sizeof(tx_resp_msg), tx_resp_msg, 0); 
                dwt_writetxfctrl(sizeof(tx_resp_msg)+FCS_LEN, 0, 1); 
                
                if (dwt_starttx(DWT_START_TX_DELAYED | DWT_RESPONSE_EXPECTED) == DWT_SUCCESS) {
                    while (!(dwt_read32bitreg(SYS_STATUS_ID) & SYS_STATUS_TXFRS_BIT_MASK)) { };
                    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
                    frame_seq_nb++;
                }
            }
        }
        else {
            dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
        }
    }
}