/**
 * usb_device.c
 * Drop-in replacement for amore_uart.c — identical public API.
 *
 * Instead of USART2, uses USB OTG FS CDC (PA11=DM, PA12=DP).
 * On the Linux side this still appears as /dev/ttyACM0.
 * server.py is unchanged.
 *
 * The baud parameter to uart_init() is ignored (USB CDC doesn't have a
 * physical baud rate — host sets line-coding but we don't enforce it).
 */
#include "amore_uart.h"
#include "usbd_cdc_if.h"
#include "usbd_desc.h"
#include "stm32f4xx_hal.h"

/* ── USB Device handle (shared with usbd_conf.c and usbd_cdc_if.c) ─────── */
USBD_HandleTypeDef hUsbDeviceFS;

/* ── Clock and GPIO init for USB OTG FS ────────────────────────────────── */
static void usb_gpio_clock_init(void) {
    /* GPIO PA11(DM), PA12(DP) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF10_OTG_FS;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* USB OTG FS clock */
    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

    /* IRQ */
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

/* ── uart_init: ignore baud, init USB CDC ───────────────────────────────── */
void uart_init(uint32_t baud) {
    (void)baud;   /* USB CDC — no physical baud rate */

    usb_gpio_clock_init();

    USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
    USBD_Start(&hUsbDeviceFS);

    /* Give host time to enumerate (important on first connect) */
    HAL_Delay(500);

    usb_cdc_start_receive();
}

/* ── uart_send: write raw bytes ─────────────────────────────────────────── */
int uart_send(const uint8_t *data, uint16_t len) {
    return usb_cdc_write(data, len, UART_TIMEOUT_MS);
}

/* ── uart_recv: blocking read of exactly `len` bytes ───────────────────── */
int uart_recv(uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    uint32_t deadline = HAL_GetTick() + timeout_ms;
    uint32_t got = 0;
    while (got < len) {
        uint32_t n = usb_cdc_read(data + got, len - got);
        got += n;
        if (got < len) {
            if (HAL_GetTick() > deadline) return -1;
        }
    }
    return 0;
}

/* ── Packet send/recv: identical to amore_uart.c ────────────────────────── */
int uart_send_packet(uint8_t cmd, const uint8_t *data, uint16_t len) {
    uint8_t hdr[5];
    hdr[0] = 0xAAu;
    hdr[1] = 0x55u;
    hdr[2] = cmd;
    hdr[3] = (uint8_t)(len & 0xFFu);
    hdr[4] = (uint8_t)(len >> 8);
    uint8_t crc = cmd ^ hdr[3] ^ hdr[4];
    for (uint16_t i = 0; i < len; i++) crc ^= data[i];

    if (uart_send(hdr, 5) != 0) return -1;
    if (len > 0 && uart_send(data, len) != 0) return -2;
    if (uart_send(&crc, 1) != 0) return -3;
    return 0;
}

int uart_recv_packet(uint8_t *cmd, uint8_t *data, uint16_t *len,
                     uint16_t max_len, uint32_t timeout_ms) {
    uint32_t deadline = HAL_GetTick() + timeout_ms;

    /* Sync: wait for 0xAA 0x55 */
    uint8_t b;
    while (1) {
        uint32_t remaining = (deadline > HAL_GetTick())
                             ? (deadline - HAL_GetTick()) : 0;
        if (uart_recv(&b, 1, remaining) != 0) return -1;
        if (b != 0xAAu) continue;
        remaining = (deadline > HAL_GetTick()) ? (deadline - HAL_GetTick()) : 0;
        if (uart_recv(&b, 1, remaining) != 0) return -1;
        if (b == 0x55u) break;
    }

    /* Header: cmd, len_lo, len_hi */
    uint8_t hdr[3];
    uint32_t rem = (deadline > HAL_GetTick()) ? (deadline - HAL_GetTick()) : 0;
    if (uart_recv(hdr, 3, rem) != 0) return -2;
    *cmd = hdr[0];
    uint16_t plen = (uint16_t)hdr[1] | ((uint16_t)hdr[2] << 8);
    if (plen > max_len) return -3;
    *len = plen;

    rem = (deadline > HAL_GetTick()) ? (deadline - HAL_GetTick()) : 0;
    if (plen > 0 && uart_recv(data, plen, rem) != 0) return -4;

    uint8_t crc_recv;
    rem = (deadline > HAL_GetTick()) ? (deadline - HAL_GetTick()) : 0;
    if (uart_recv(&crc_recv, 1, rem) != 0) return -5;

    uint8_t crc = hdr[0] ^ hdr[1] ^ hdr[2];
    for (uint16_t i = 0; i < plen; i++) crc ^= data[i];
    return (crc == crc_recv) ? 0 : -6;
}
