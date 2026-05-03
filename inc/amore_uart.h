#pragma once
#include <stdint.h>

/* USART2 on PA2(TX)/PA3(RX) — bridges to ST-Link VCP on V2J37+ firmware.
 * Packet format: [0xAA][0x55][CMD:1][LEN_LO:1][LEN_HI:1][DATA:LEN][CRC8:1]
 * CRC8 = XOR of CMD + LEN_LO + LEN_HI + all DATA bytes. */

#define UART_CMD_SETUP   0x10u   /* Client → Server: pub = A,B,C,D      (384 B) */
#define UART_CMD_RESULT  0x20u   /* Server → Client: out = gamma,rho    (768 B) */
#define UART_CMD_STATUS  0x30u   /* Client → Server: 1 byte (0=fail,1=ok)       */
#define UART_CMD_READY   0x40u   /* Server → Client: 1 byte mode (0=honest,1=mal) */

#define UART_TIMEOUT_MS  120000u  /* 10-second timeout for server response */

void     uart_init(uint32_t baud);
int      uart_send(const uint8_t *data, uint16_t len);  /* 0=ok */
int      uart_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms); /* 0=ok */
int      uart_send_packet(uint8_t cmd, const uint8_t *data, uint16_t len);
int      uart_recv_packet(uint8_t *cmd, uint8_t *data, uint16_t *len,
                          uint16_t max_len, uint32_t timeout_ms);
