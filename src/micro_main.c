/* =========================================================================
 *  micro_main.c — entry point for the FAIR primitive microbench.
 *
 *  Mirrors pairing_main.c EXACTLY (same 168 MHz clock, same DWT init) so the
 *  RELIC primitives in micro_bench.c are timed under identical conditions to
 *  the pp_map_oatep_k12 baseline. Separate main() — CMake builds its own ELF.
 *
 *  After the run, read results in GDB:   (gdb) print g_micro
 * ========================================================================= */

#include "stm32f4xx_hal.h"
#include "triggers.h"
#include <stdint.h>

/* micro_bench.c owns the struct; main only needs to start it and read status */
extern void     Micro_Run(void);
extern uint32_t Micro_Status(void);   /* returns g_micro.status */

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 8;
    osc.PLL.PLLN            = 168;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 7;
    HAL_RCC_OscConfig(&osc);

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5);

    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
}

static void LED_Init(void) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &g);
}
static void LED_Set(uint32_t pattern) {
    GPIOD->BSRR = ((~pattern & 0xFu) << (12 + 16)) | ((pattern & 0xFu) << 12);
}
static void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    DWT_Init();
    Triggers_Init();

    LED_Set(0x1);            /* booting */
    Micro_Run();             /* fills g_micro */

    if (Micro_Status() == 0x600D0000u) LED_Set(0xF);   /* success */
    else                               LED_Set(0x5);   /* error   */

    uint32_t s = 0;
    for (;;) { HAL_Delay(500); s ^= 0x1u; LED_Set(s); }
}

void NMI_Handler(void)        { while(1); }
void HardFault_Handler(void)  { LED_Set(0x2); while(1); }
void MemManage_Handler(void)  { LED_Set(0x3); while(1); }
void BusFault_Handler(void)   { LED_Set(0x6); while(1); }
void UsageFault_Handler(void) { LED_Set(0x9); while(1); }
