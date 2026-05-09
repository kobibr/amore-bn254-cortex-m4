/**
 * usbd_conf.c
 * USB HAL PCD callbacks — maps USB Device Library onto STM32F4 OTG FS hardware.
 * USB OTG FS pins: PA11=DM, PA12=DP.  Clock: 48 MHz from PLL.
 */
#include "usbd_core.h"
#include "stm32f4xx_hal.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* ── IRQ handler ────────────────────────────────────────────────────────── */
void OTG_FS_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); }

/* ── HAL PCD callbacks → USB Device Library ────────────────────────────── */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup); }

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
    { USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff); }

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
    { USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff); }

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_SOF(hpcd->pData); }

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    USBD_LL_SetSpeed(hpcd->pData, speed);
    USBD_LL_Reset(hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_Suspend(hpcd->pData); }

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_Resume(hpcd->pData); }

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
    { USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum); }

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
    { USBD_LL_IsoINIncomplete(hpcd->pData, epnum); }

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_DevConnected(hpcd->pData); }

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
    { USBD_LL_DevDisconnected(hpcd->pData); }

/* ── USBD LL interface ──────────────────────────────────────────────────── */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
    hpcd_USB_OTG_FS.Instance                = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints      = 4;
    hpcd_USB_OTG_FS.Init.speed              = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable         = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface         = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable         = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable   = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable         = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1  = DISABLE;
    hpcd_USB_OTG_FS.pData = pdev;
    pdev->pData            = &hpcd_USB_OTG_FS;

    HAL_PCD_Init(&hpcd_USB_OTG_FS);

    /* FIFO sizes (words): RX=128, EP0-IN=64, EP1-IN=64, EP2-IN=64, EP3-IN=64 */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 3, 0x40);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    HAL_PCD_DeInit(pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Start(pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
    HAL_PCD_Stop(pdev->pData);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev,
    uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_Close(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_Flush(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);
    return USBD_OK;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    PCD_HandleTypeDef *hpcd = pdev->pData;
    return (ep_addr & 0x80)
        ? hpcd->IN_ep[ep_addr & 0x7F].is_stall
        : hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
    HAL_PCD_SetAddress(pdev->pData, dev_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev,
    uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
    uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
    return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay) { HAL_Delay(Delay); }

/* malloc/free for USB Device Library */
void *USBD_static_malloc(uint32_t size) {
    static uint8_t mem[512];
    (void)size;
    return mem;
}
void USBD_static_free(void *p) { (void)p; }
