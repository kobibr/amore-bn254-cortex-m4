#pragma once
#include <stdint.h>
#include "usbd_cdc.h"

/* CDC Application interface (usbd_cdc_if.c) */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint32_t usb_cdc_available(void);
uint32_t usb_cdc_read(uint8_t *dst, uint32_t len);
int      usb_cdc_write(const uint8_t *src, uint32_t len, uint32_t timeout_ms);
void     usb_cdc_start_receive(void);
