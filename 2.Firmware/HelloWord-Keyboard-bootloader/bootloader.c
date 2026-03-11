#include "stm32f1xx.h"
#include "usb_hid.h"

/*
 * Recovery architecture (3 layers):
 *
 * Layer 1 - Magic word: App sends HID cmd 0xDF → writes magic → resets → bootloader
 * Layer 2 - Boot watchdog flag: Bootloader sets flag before jumping to app.
 *           App must clear flag within first second. If app crashes and triggers
 *           a soft reset (HardFault/watchdog), flag survives → bootloader.
 * Layer 3 - App validity check: If no valid stack pointer at APP_ADDRESS → bootloader
 *
 * All flags stored in STM32F1 BKP data registers (survive reset, immune to
 * stack corruption).
 */

#define DFU_MAGIC_WORD  0xB011U
#define BOOT_FLAG_WORD  0xDEADU

#define SRAM_END        0x20005000U

static void bkp_unlock(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
}

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

    bkp_unlock();

    /* Layer 1: Explicit DFU request from app (HID cmd 0xDF) */
    if (BKP->DR1 == DFU_MAGIC_WORD)
    {
        BKP->DR1 = 0;
        enter_dfu = 1;
    }

    /* Layer 2: Boot watchdog - app failed to clear flag after last boot */
    if (BKP->DR2 == BOOT_FLAG_WORD)
    {
        BKP->DR2 = 0;
        enter_dfu = 1;
    }

    /* Layer 3: No valid application in flash */
    if (!enter_dfu && !app_is_valid())
        enter_dfu = 1;

    if (!enter_dfu)
    {
        /* Set boot watchdog flag; app must clear it to prove it booted OK */
        BKP->DR2 = BOOT_FLAG_WORD;
        jump_to_app();
    }

    clock_init();
    usb_hid_init();

    while (1)
    {
        usb_hid_poll();

        if (usb_hid_is_complete())
        {
            for (volatile int i = 0; i < 500000; i++);
            NVIC_SystemReset();
        }
    }
}
