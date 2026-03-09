/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint64_t serialNumber;
char serialNumberStr[13];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

    /*
     * Early-boot DFU recovery — bare registers, zero HAL dependency.
     * Runs BEFORE HAL_Init / clocks / GPIO, so even a completely broken
     * firmware can't prevent this from executing.
     */
    {
        /* Use RTC backup registers — immune to MSP stack corruption */
        RCC->APB1ENR |= RCC_APB1ENR_PWREN;
        (void)RCC->APB1ENR;
        PWR->CR |= PWR_CR_DBP;
        (void)PWR->CR;

        #define DFU_MAGIC   RTC->BKP0R
        #define BOOT_FLAG   RTC->BKP1R
        int enter_dfu = 0;

        if (DFU_MAGIC == 0xB00110ADU)  { DFU_MAGIC = 0; enter_dfu = 1; }
        if (BOOT_FLAG == 0xDEADC0DEU)  { BOOT_FLAG = 0; enter_dfu = 1; }

        /* KEY_A=PC8, KEY_B=PC9 — check with bare register access */
        if (!enter_dfu)
        {
            RCC->AHB1ENR |= (1U << 2);                /* GPIOC clock ON */
            (void)RCC->AHB1ENR;                        /* settle */
            GPIOC->PUPDR = (GPIOC->PUPDR & ~((3U<<16)|(3U<<18)))
                         | ((1U<<16)|(1U<<18));        /* PC8,PC9 pull-up */
            for (volatile int i = 0; i < 5000; i++);   /* debounce */
            if (!(GPIOC->IDR & (1U<<8)) && !(GPIOC->IDR & (1U<<9)))
                enter_dfu = 1;
        }

        if (enter_dfu)
        {
            __disable_irq();
            SysTick->CTRL = 0;
            for (int i = 0; i < 8; i++) {
                NVIC->ICER[i] = 0xFFFFFFFFU;
                NVIC->ICPR[i] = 0xFFFFFFFFU;
            }

            /* Force USB disconnect so host re-enumerates cleanly */
            RCC->AHB2ENR |= (1U << 7);     /* USB OTG FS clock ON */
            RCC->AHB1ENR |= (1U << 0);     /* GPIOA clock ON */
            (void)RCC->AHB1ENR;
            /* PA12 (USB D+) as open-drain output LOW → host sees disconnect */
            GPIOA->MODER  = (GPIOA->MODER & ~(3U << 24)) | (1U << 24);
            GPIOA->OTYPER |= (1U << 12);
            GPIOA->BSRR    = (1U << 28);   /* reset PA12 LOW */
            for (volatile int d = 0; d < 200000; d++);  /* ~50ms disconnect */

            /* Reset clocks to default HSI state */
            RCC->CR |= RCC_CR_HSION;
            while (!(RCC->CR & RCC_CR_HSIRDY));
            RCC->CFGR = 0;
            RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);

            RCC->APB2ENR |= (1U << 14);    /* SYSCFG clock */
            (void)RCC->APB2ENR;
            SYSCFG->MEMRMP = 0x01;          /* remap system flash to 0x0 */

            SCB->VTOR = 0x1FFF0000U;
            __set_MSP(*(volatile uint32_t*)0x1FFF0000U);
            ((void(*)(void))(*(volatile uint32_t*)0x1FFF0004U))();
            while(1);
        }
    }

HAL_RCC_DeInit();
    // This procedure of building a USB serial number should be identical
    // to the way the STM's built-in USB bootloader does it. This means
    // that the device will have the same serial number in normal and DFU mode.
    uint32_t uuid0 = *(uint32_t *) (UID_BASE + 0);
    uint32_t uuid1 = *(uint32_t *) (UID_BASE + 4);
    uint32_t uuid2 = *(uint32_t *) (UID_BASE + 8);
    uint32_t uuid_mixed_part = uuid0 + uuid2;
    serialNumber = ((uint64_t) uuid_mixed_part << 16) | (uint64_t) (uuid1 >> 16);

    uint64_t val = serialNumber;
    for (size_t i = 0; i < 12; ++i)
    {
        serialNumberStr[i] = "0123456789ABCDEF"[(val >> (48 - 4)) & 0xf];
        val <<= 4;
    }
    serialNumberStr[12] = 0;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
    HAL_RCC_DeInit();

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART2_UART_Init();
  MX_TIM9_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
    /* Boot flag in RTC backup register — Main() must clear it to prove app is healthy */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP1R = 0xDEADC0DEU;
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in freertos.c) */
  MX_FREERTOS_Init();
  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    while (1)
    {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void OnTimerCallback(TIM_TypeDef *timInstance);
/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  else
  {
      OnTimerCallback(htim->Instance);
  }
  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
