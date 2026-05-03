/**
 * startup_stm32f407xx.c
 * Minimal C startup for STM32F407
 */

#include <stdint.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

int main(void);
void SystemInit(void);

void Default_Handler(void) { while(1) {} }

/* SysTick_Handler must call HAL_IncTick so that HAL_Delay works.
   Without this, every HAL_Delay call would trigger the SysTick interrupt,
   land in Default_Handler, and hang the firmware. */
extern void HAL_IncTick(void);
void SysTick_Handler(void) { HAL_IncTick(); }

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));

void Reset_Handler(void) {
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;
    SystemInit();
    main();
    while(1) {}
}

__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    (void (*)(void))((uint32_t)&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,   /* routed to HAL_IncTick (not Default_Handler) */
};
