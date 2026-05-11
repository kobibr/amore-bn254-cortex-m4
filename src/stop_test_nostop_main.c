/*
 * stop_test_nostop_main.c — functional smoke test of the stop_test framework
 * WITHOUT actually entering Stop mode.
 *
 * Replaces LowPower_EnterStopAndWait() with HAL_Delay(5000), keeping the
 * rest of the structure identical. Validates:
 *   - PA0 toggles correctly via TRIG_COMPUTE_HI/LO
 *   - LED pattern visible on PD12-15
 *   - gpio_logger on Pi captures the 12 expected transitions
 *
 * The real Stop-mode test is deferred until PPK2 arrives (then we have
 * actual µA-level current measurement to validate the floor).
 */
#include "stm32f4xx_hal.h"
#include "triggers.h"

#define NOSTOP_CYCLES   6u
#define RUN_MS         100u
#define WAIT_MS       5000u

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

int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Triggers_Init();

    LED_Set(0x1);   /* PD12 on = booting */
    HAL_Delay(500); /* 0.5s settle so gpio_logger sees clean low */

    for (uint32_t i = 0; i < NOSTOP_CYCLES; i++) {
        TRIG_COMPUTE_HI();
        LED_Set(0xF);
        HAL_Delay(RUN_MS);
        LED_Set(0x0);
        TRIG_COMPUTE_LO();
        HAL_Delay(WAIT_MS);
    }

    while (1) {
        LED_Set(0x1);
        HAL_Delay(500);
        LED_Set(0x0);
        HAL_Delay(500);
    }
}

void NMI_Handler(void)      { while(1); }
void HardFault_Handler(void){ while(1); }
void MemManage_Handler(void){ while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void){ while(1); }
