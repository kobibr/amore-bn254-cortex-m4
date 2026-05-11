#include "stm32f4xx_hal.h"
#include "amore.h"
#include "triggers.h"
#include <string.h>

/* Results stored in a known symbol so GDB can read them */
AmorE_BenchResults g_results;

/* -----------------------------------------------------------------------
 * System Clock Config — 168 MHz via PLL from HSI (16 MHz)
 * AHB=168, APB1=42, APB2=84
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

    /* FLASH prefetch + instruction cache */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
}

/* -----------------------------------------------------------------------
 * LED on PD12 (green LED on STM32F4-Discovery)
 * ----------------------------------------------------------------------- */
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
    /* pattern bits 0-3 → PD12-PD15 */
    GPIOD->BSRR = ((~pattern & 0xFu) << (12 + 16)) | ((pattern & 0xFu) << 12);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();
    Triggers_Init();

    /* Signal start: green on */
    LED_Set(0x1);

    /* Run benchmark — this blocks until complete */
    AmorE_RunBenchmark(&g_results);

    /* Signal result on LEDs: 0xF = all 4 green (pass), 0x5 = error */
    if (g_results.status == 0x600D0000u) {
        LED_Set(0xF);   /* all LEDs = success */
    } else {
        LED_Set(0x5);   /* pattern = error */
    }

    /* Spin here — GDB can inspect g_results */
    uint32_t led_state = 0;
    for (;;) {
        HAL_Delay(500);
        led_state ^= 0x1u;
        LED_Set(led_state);
    }
}

/* HAL required weak callbacks
 * SysTick_Handler is defined in startup_stm32f407xx.c — do NOT redefine here */
void NMI_Handler(void)      { while(1); }
void HardFault_Handler(void){ LED_Set(0x2); while(1); }
void MemManage_Handler(void){ while(1); }
void BusFault_Handler(void) { while(1); }
void UsageFault_Handler(void){ while(1); }
