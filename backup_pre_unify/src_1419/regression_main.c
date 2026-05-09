/* =========================================================================
 *  regression_main.c — entry point of regression_test.elf
 *
 *  Mirrors pairing_main.c structure exactly: clock init, LED, DWT,
 *  then dispatches to the regression test routine.
 *
 *  Author: Kobi Brener <kob.tov@gmail.com>
 * ========================================================================= */

#include "stm32f4xx_hal.h"
#include "regression_test.h"
#include <string.h>

/* Global readable by GDB */
regression_results_t g_reg_results;

/* -----------------------------------------------------------------------
 *  System Clock Config — IDENTICAL to pairing_main.c / AmorE main.c
 *  168 MHz via PLL from HSI 16 MHz
 * ----------------------------------------------------------------------- */
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

    LED_Set(0x1);  /* PD12 (green) on = booting */

    /* Run the regression test suite */
    regression_run(&g_reg_results);

    /* Indicate result via LEDs */
    if (g_reg_results.status == REGRESSION_STATUS_OK) {
        LED_Set(0xF);   /* all 4 LEDs = success */
    } else {
        LED_Set(0x5);   /* alternating = failure (LD3+LD5) */
    }

    /* Halt — debugger reads g_reg_results */
    for (;;) {
        __asm__("nop");
    }
}
