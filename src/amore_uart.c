#include "amore_uart.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static UART_HandleTypeDef huart2;

void uart_init(uint32_t baud) {
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = baud;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

int uart_send(const uint8_t *data, uint16_t len) {
    return (HAL_UART_Transmit(&huart2, (uint8_t *)data, len, UART_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

int uart_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    return (HAL_UART_Receive(&huart2, data, len, timeout_ms) == HAL_OK) ? 0 : -1;
}

int uart_send_packet(uint8_t cmd, const uint8_t *data, uint16_t len) {
    /* Build header */
    uint8_t hdr[5];
    hdr[0] = 0xAAu;
    hdr[1] = 0x55u;
    hdr[2] = cmd;
    hdr[3] = (uint8_t)(len & 0xFFu);
    hdr[4] = (uint8_t)(len >> 8);
    /* Compute CRC8 = XOR of cmd, len bytes, and all data */
    uint8_t crc = cmd ^ hdr[3] ^ hdr[4];
    for (uint16_t i = 0; i < len; i++) crc ^= data[i];

    if (uart_send(hdr, 5) != 0) return -1;
    if (len > 0 && uart_send(data, len) != 0) return -2;
    if (uart_send(&crc, 1) != 0) return -3;
    return 0;
}

int uart_recv_packet(uint8_t *cmd, uint8_t *data, uint16_t *len,
                     uint16_t max_len, uint32_t timeout_ms) {
    uint8_t sync[2];
    /* Wait for sync bytes 0xAA 0x55 */
    while (1) {
        if (uart_recv(sync, 1, timeout_ms) != 0) return -1;
        if (sync[0] != 0xAAu) continue;
        if (uart_recv(sync+1, 1, timeout_ms) != 0) return -1;
        if (sync[1] == 0x55u) break;
    }
    uint8_t hdr[3];
    if (uart_recv(hdr, 3, timeout_ms) != 0) return -2;  /* cmd, len_lo, len_hi */
    /* Bug #8 fix: previously `*cmd = hdr[0]` was written here, BEFORE the
     * plen-overflow check. Callers that didn't strictly check the return
     * code could observe a stale/garbage *cmd from a packet that was
     * actually rejected. Defer all output-pointer writes until every
     * validation has passed. */
    uint16_t plen = (uint16_t)hdr[1] | ((uint16_t)hdr[2] << 8);
    if (plen > max_len) return -3;
    if (plen > 0 && uart_recv(data, plen, timeout_ms) != 0) return -4;
    uint8_t crc_recv;
    if (uart_recv(&crc_recv, 1, timeout_ms) != 0) return -5;
    /* Verify CRC */
    uint8_t crc = hdr[0] ^ hdr[1] ^ hdr[2];
    for (uint16_t i = 0; i < plen; i++) crc ^= data[i];
    if (crc != crc_recv) return -6;
    /* All checks passed — now publish outputs atomically. */
    *cmd = hdr[0];
    *len = plen;
    return 0;
}
