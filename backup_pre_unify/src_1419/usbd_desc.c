/**
 * usbd_desc.c
 * USB CDC device/configuration/string descriptors.
 * Appears on Linux as /dev/ttyACM0 (same as before — server.py unchanged).
 */
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"
#include <string.h>

#define USBD_VID            0x0483u   /* STMicroelectronics */
#define USBD_PID_CDC        0x5740u   /* STM32 Virtual COM Port */
#define USBD_LANGID_STRING  0x0409u   /* English */
#define USBD_MAX_STR_DESC   512u

static void Get_SerialNum(void);
static uint8_t *USBD_CDC_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *USBD_CDC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);

USBD_DescriptorsTypeDef CDC_Desc = {
    USBD_CDC_DeviceDescriptor,
    USBD_CDC_LangIDStrDescriptor,
    USBD_CDC_ManufacturerStrDescriptor,
    USBD_CDC_ProductStrDescriptor,
    USBD_CDC_SerialStrDescriptor,
    USBD_CDC_ConfigStrDescriptor,
    USBD_CDC_InterfaceStrDescriptor,
};

/* USB Standard Device Descriptor */
static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] = {
    0x12,                       /* bLength */
    USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
    0x00, 0x02,                 /* bcdUSB = 2.00 */
    0x02,                       /* bDeviceClass: CDC */
    0x00,                       /* bDeviceSubClass */
    0x00,                       /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,           /* bMaxPacketSize */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID_CDC), HIBYTE(USBD_PID_CDC),
    0x00, 0x02,                 /* bcdDevice = 2.00 */
    USBD_IDX_MFC_STR,           /* iManufacturer */
    USBD_IDX_PRODUCT_STR,       /* iProduct */
    USBD_IDX_SERIAL_STR,        /* iSerialNumber */
    USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

static uint8_t USBD_StringSerial[0x1Au] = {
    0x1Au, USB_DESC_TYPE_STRING,
};

static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC];

static uint8_t *USBD_CDC_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

static uint8_t *USBD_CDC_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *USBD_CDC_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)"AmorE BN128", USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_CDC_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)"AmorE STM32 CDC", USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_CDC_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = 0x1Au;
    Get_SerialNum();
    return USBD_StringSerial;
}

static uint8_t *USBD_CDC_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)"AmorE CDC Config", USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *USBD_CDC_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)"AmorE CDC Interface", USBD_StrDesc, length);
    return USBD_StrDesc;
}

/* Build serial number string from STM32 UID registers */
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len) {
    for (int i = 0; i < len; i++) {
        uint8_t nib = (value >> ((len-1-i)*4)) & 0x0Fu;
        pbuf[2*i]   = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
        pbuf[2*i+1] = 0;
    }
}

static void Get_SerialNum(void) {
    uint32_t uid0 = *(volatile uint32_t *)0x1FFF7A10u;
    uint32_t uid1 = *(volatile uint32_t *)0x1FFF7A14u;
    uint32_t uid2 = *(volatile uint32_t *)0x1FFF7A18u;
    uid0 += uid2;
    IntToUnicode(uid0, &USBD_StringSerial[2],  8);
    IntToUnicode(uid1, &USBD_StringSerial[18], 4);
}
