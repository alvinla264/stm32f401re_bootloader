/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "flash_app_handler.h"
#include "ota_update.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VERSION_MAJOR 0
#define VERSION_MINOR 3
#define NEW_FIRMWARE  0x34

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
const uint8_t BL_Version[2] = {VERSION_MAJOR, VERSION_MINOR};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void goto_application(uint32_t app_base_addr);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
	(void)file;
	int DataIdx;

	for (DataIdx = 0; DataIdx < len; DataIdx++)
	{
		//__io_putchar(*ptr++);
		if (HAL_UART_Transmit(&huart2, (uint8_t *)ptr++, 1, HAL_MAX_DELAY) != HAL_OK)
			return DataIdx;
	}
	return len;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	/* USER CODE BEGIN 2 */
	printf("Starting Bootloader v%d.%d\r\n", BL_Version[0], BL_Version[1]);
	uint8_t rcv_buff = 0;
	Flash_Config_t curr_config = {0};
	//Gets the current configuration stored in the Flash Memory
	if (Flash_GetConfig(&curr_config) != FLASH_APP_OK)
	{
		printf("Error getting current config from flash\r\n");
		Error_Handler();
	}
	//When Full Flash Erase the value in the Flash should be 0xFFFFFFFF
	//Anything else means someone could've changed the value
	if (curr_config.first_boot != FIRST_BOOT_FALSE && curr_config.first_boot != FIRST_BOOT_TRUE)
	{
		// Possible Tamper detected??
		// TODO implement something here
		printf("Unknown first boot\r\n");
		curr_config.first_boot = FIRST_BOOT_TRUE;
	}
	//When application is running and needs to go back into
	//boot mode for new firmware, this flag can be set
	//in the RTC backup register and then do a software reset
	uint8_t firmware_flag_set = RTC->BKP0R & 0x01;
	//Checks for normal boot and waits for any UART commands being sent
	if (curr_config.first_boot == FIRST_BOOT_FALSE && !firmware_flag_set)
	{
		printf("Listening for UART Commands\r\n");
		HAL_UART_Receive(&huart2, &rcv_buff, 1, 3000);
	}
	//Firmware update mode
	if (firmware_flag_set || rcv_buff == NEW_FIRMWARE || curr_config.first_boot == FIRST_BOOT_TRUE)
	{
		if (curr_config.first_boot == FIRST_BOOT_TRUE)
		{
			printf("First Boot Detected\r\n");
		}
		//clears the application firmware_flag
		if (firmware_flag_set)
		{
			printf("New Firmware Flag Set\r\n");
			PWR->CR |= PWR_CR_DBP;
			RTC->BKP0R = (uint32_t)0;
			PWR->CR &= ~PWR_CR_DBP;
		}
		printf("Starting firmware download\r\n");
		//in case invalid address memory in Flash Config, set to SLOT0
		if (curr_config.curr_app_slot != APP_SLOT0_ADDR && curr_config.curr_app_slot != APP_SLOT1_ADDR)
			curr_config.curr_app_slot = APP_SLOT1_ADDR;
		uint32_t new_app_addr = (curr_config.first_boot == FIRST_BOOT_TRUE)		? APP_SLOT0_ADDR
								: (curr_config.curr_app_slot == APP_SLOT0_ADDR) ? APP_SLOT1_ADDR
																				: APP_SLOT0_ADDR;
		if (new_app_addr == APP_SLOT0_ADDR)
		{
			printf("Writing to Slot0\r\n");
		}
		else
		{
			printf("Writing to Slot1\r\n");
		}
		//downloads and flash the firmware
		if (ota_download_and_flash(new_app_addr) != OTA_OK)
		{
			printf("OTA Update: ERROR!!\r\n");
			printf("Rebooting and running previous Application\r\n");
			HAL_NVIC_SystemReset();
		}
		else
		{
			//if sucessful calculate CRC using STM32 CRC peripheral
			uint8_t crc_value = 0;
			if (Flash_CalculateCRC(new_app_addr, &crc_value) != FLASH_APP_OK)
			{
				printf("Invalid Address for CRC Calculation\r\n");
				Error_Handler();
			}
			//sets new configuration
			curr_config.curr_app_slot = new_app_addr;
			curr_config.first_boot = FIRST_BOOT_FALSE;
			// TODO: CALCULATE CRC

			if (new_app_addr == APP_SLOT0_ADDR)
			{
				curr_config.slot0_crc = crc_value;
			}
			else
			{
				curr_config.slot1_crc = crc_value;
			}
			//writes new config into the FLASH memory
			if (Flash_WriteConfig(curr_config) != FLASH_APP_OK)
			{
				printf("Error writing new config\r\n");
				Error_Handler();
			}
			printf("Firmware update is completed! Rebooting ... \r\n");
			HAL_NVIC_SystemReset();
		}
	}
	//in case invalid memory stored in FLASH
	if (curr_config.curr_app_slot != APP_SLOT0_ADDR && curr_config.curr_app_slot != APP_SLOT1_ADDR)
	{
		curr_config.curr_app_slot = APP_SLOT0_ADDR;
	}
	//CRC tamper detection
	uint8_t slot_crc = 0;
	if (curr_config.curr_app_slot == APP_SLOT0_ADDR)
	{
		slot_crc = curr_config.slot0_crc;
	}
	else
	{
		slot_crc = curr_config.slot1_crc;
	}
	uint8_t crc = 0;
	//Calculates Flash Appliction SLOT CRC for tamper detection
	if (Flash_CalculateCRC(curr_config.curr_app_slot, &crc) != FLASH_APP_OK)
	{
		printf("Invalid Address for CRC Calculation\r\n");
		Error_Handler();
	}
	//compares calculated CRC to stored CRC
	if (crc != slot_crc)
	{
		printf("TAMPER DETECTED!!!\r\n");
		if (curr_config.curr_app_slot == APP_SLOT0_ADDR)
		{
			printf("Slot 0 CRC does not match calculated CRC\r\n");
		}
		else
		{
			printf("Slot 1 CRC does not match calculated CRC\r\n");
		}
		//TODO: Change Error Handler to find previous valid SLOT #
		Error_Handler();
	}
	goto_application(curr_config.curr_app_slot);
	/* USER CODE END 2 */

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
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LD2_Pin */
	GPIO_InitStruct.Pin = LD2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void goto_application(uint32_t app_base_addr)
{
	if (app_base_addr == APP_SLOT0_ADDR)
	{
		printf("Going to SLOT0 application\r\n");
	}
	else
	{
		printf("Going to SLOT1 application\r\n");
	}
	void (*app_handler)(void) = (void (*)(void))(*(volatile uint32_t *)(app_base_addr + 4));

	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
	HAL_Delay(500);
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
	//__disable_irq();
	HAL_UART_DeInit(&huart2);
	HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
	HAL_GPIO_DeInit(LD2_GPIO_Port, LD2_Pin);
	HAL_DeInit();
	__set_MSP(*(volatile uint32_t *)app_base_addr);
	app_handler();
}
/* USER CODE END 4 */

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

#ifdef USE_FULL_ASSERT
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
	/* User can add his own implementation to report the file name and line
	   number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
	   line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
