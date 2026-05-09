/**
 * usbd_cdc_if.c
 * CDC Application Interface — ring-buffer between USB interrupts and amore.c.
 *
 * TX: write bytes → usb_cdc_write() → transmitted via USB CDC IN endpoint
 * RX: bytes arrive via USB interrupt → stored in ring buffer → read by usb_cdc_read()
 *
 * Both sides are safe to call from the main thread; USB IRQ only writes to rx_buf.
 */
#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include <string.h>

/* ── RX ring buffer ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE  2048u   /* must be power of 2 */
#define TX_BUF_SIZE  1024u
#define USB_FS_MAX_PACKET 64u

static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head = 0;   /* written by IRQ */
static volatile uint32_t rx_tail = 0;   /* read  by main  */

static uint8_t tx_buf[TX_BUF_SIZE];     /* scratch for transmit */
static uint8_t usb_rx_fs_buf[USB_FS_MAX_PACKET];  /* USB OUT staging */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* ── CDC callbacks ─────────────────────────────────────────────────────── */
static int8_t CDC_Init_FS(void)     { return USBD_OK; }
static int8_t CDC_DeInit_FS(void)   { return USBD_OK; }
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
    (void)cmd; (void)pbuf; (void)length;
    return USBD_OK;
}

/* Called from USB IRQ when data arrives on OUT endpoint */
static int8_t CDC_Receive_FS(uint8_t *buf, uint32_t *len) {
    uint32_t n = *len;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t next = (rx_head + 1) & (RX_BUF_SIZE - 1);
        if (next != rx_tail) {           /* discard if full */
            rx_buf[rx_head] = buf[i];
            rx_head = next;
        }
    }
    /* Re-arm receive */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_rx_fs_buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *len, uint8_t epnum) {
    (void)pbuf; (void)len; (void)epnum;
    return USBD_OK;
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS, CDC_DeInit_FS, CDC_Control_FS,
    CDC_Receive_FS, CDC_TransmitCplt_FS
};

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Return number of bytes available to read */
uint32_t usb_cdc_available(void) {
    return (rx_head - rx_tail) & (RX_BUF_SIZE - 1);
}

/* Read up to `len` bytes. Returns bytes actually read. */
uint32_t usb_cdc_read(uint8_t *dst, uint32_t len) {
    uint32_t avail = usb_cdc_available();
    if (len > avail) len = avail;
    for (uint32_t i = 0; i < len; i++) {
        dst[i]  = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);
    }
    return len;
}

/* Write `len` bytes. Blocks until all sent or timeout. Returns 0 on success. */
int usb_cdc_write(const uint8_t *src, uint32_t len, uint32_t timeout_ms) {
    extern uint32_t HAL_GetTick(void);
    uint32_t deadline = HAL_GetTick() + timeout_ms;

    while (len > 0) {
        uint32_t chunk = (len > TX_BUF_SIZE) ? TX_BUF_SIZE : len;
        memcpy(tx_buf, src, chunk);

        /* Wait for previous TX to complete */
        USBD_CDC_HandleTypeDef *hcdc =
            (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
        while (hcdc && hcdc->TxState != 0) {
            if (HAL_GetTick() > deadline) return -1;
        }

        if (USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_buf, (uint16_t)chunk) != USBD_OK)
            return -2;
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK)
            return -3;

        src += chunk;
        len -= chunk;
    }
    return 0;
}

/* Called once at startup to arm the first receive */
void usb_cdc_start_receive(void) {
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_rx_fs_buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}
