#pragma once
/* usbd_conf.h — USB Device Library low-level configuration
 * Required by STM32_USB_Device_Library middleware (usbd_core.h includes it).
 * This file maps USB HAL types and defines max endpoints/strings. */

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Max number of endpoints (IN + OUT, each direction) */
#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       512U
#define USBD_SUPPORT_USER_STRING_DESC  0U
#define USBD_SELF_POWERED           1U
#define USBD_DEBUG_LEVEL            0U

/* CDC specific */
#define USBD_CDC_INTERVAL           2000U   /* polling interval ms */

/* Memory management — use static allocation */
#define USBD_malloc         USBD_static_malloc
#define USBD_free           USBD_static_free
#define USBD_memset         memset
#define USBD_memcpy         memcpy
#define USBD_Delay          HAL_Delay

void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

/* DEBUG macros (disabled) */
#if (USBD_DEBUG_LEVEL > 0)
#define USBD_UsrLog(...)   printf(__VA_ARGS__)
#else
#define USBD_UsrLog(...)
#endif
#if (USBD_DEBUG_LEVEL > 1)
#define USBD_ErrLog(...)   printf(__VA_ARGS__)
#else
#define USBD_ErrLog(...)
#endif
#if (USBD_DEBUG_LEVEL > 2)
#define USBD_DbgLog(...)   printf(__VA_ARGS__)
#else
#define USBD_DbgLog(...)
#endif
