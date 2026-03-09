#include "stm32f1xx.h"
#include "usb_dfu.h"

/*
 * Recovery architecture (3 layers):
 *
 * Layer 1 - DFU magic word: App sends HID cmd 0xDF → writes magic → resets → DFU
 * Layer 2 - Boot watchdog flag: Bootloader sets flag before jumping to app.
 *           App must clear flag within first second. If app crashes and triggers
 *           a soft reset (HardFault/watchdog), flag survives in RAM → DFU.
 * Layer 3 - App validity check: If no valid stack pointer at APP_ADDRESS → DFU
 *
 * Worst case (app hangs without crashing): IWDG in the app forces a hardware
 * reset after ~4s, RAM is preserved, Layer 2 catches it on next boot.
 */

#define MAGIC_WORD      0xB00110ADU
#define MAGIC_ADDR      (*(volatile uint32_t*)0x20004FF0U)

#define BOOT_FLAG_WORD  0xDEADC0DEU
#define BOOT_FLAG_ADDR  (*(volatile uint32_t*)0x20004FECU)

#define SRAM_END        0x20005000U

static void clock_init(void)
{
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CFGR = RCC_CFGR_PLLSRC
              | RCC_CFGR_PLLMULL9
              | RCC_CFGR_PPRE1_DIV2;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static int app_is_valid(void)
{
    uint32_t sp = *(volatile uint32_t*)APP_ADDRESS;
    return (sp >= 0x20000000U && sp <= SRAM_END);
}

static void jump_to_app(void)
{
    uint32_t app_sp    = *(volatile uint32_t*)(APP_ADDRESS);
    uint32_t app_entry = *(volatile uint32_t*)(APP_ADDRESS + 4U);

    RCC->APB1ENR = 0;
    RCC->APB2ENR = 0;
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
    RCC->CFGR = 0;

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    for (int i = 0; i < 2; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = APP_ADDRESS;
    __set_MSP(app_sp);
    ((void (*)(void))app_entry)();
    while (1);
}

void SystemInit(void) {}

int main(void)
{
    int enter_dfu = 0;

    /* Layer 1: Explicit DFU request from app (HID cmd 0xDF) */
    if (MAGIC_ADDR == MAGIC_WORD)
    {
        MAGIC_ADDR = 0;
        enter_dfu = 1;
    }

    /* Layer 2: Boot watchdog - app failed to clear flag after last boot */
    if (BOOT_FLAG_ADDR == BOOT_FLAG_WORD)
    {
        BOOT_FLAG_ADDR = 0;
        enter_dfu = 1;
    }

    /* Layer 3: No valid application in flash */
    if (!enter_dfu && !app_is_valid())
        enter_dfu = 1;

    if (!enter_dfu)
    {
        /* Set boot watchdog flag; app must clear it to prove it booted OK */
        BOOT_FLAG_ADDR = BOOT_FLAG_WORD;
        jump_to_app();
    }

    /* Enter DFU mode */
    clock_init();
    usb_dfu_init();

    while (1)
    {
        usb_dfu_poll();

        if (usb_dfu_is_complete())
        {
            for (volatile int i = 0; i < 500000; i++);
            NVIC_SystemReset();
        }
    }
}
