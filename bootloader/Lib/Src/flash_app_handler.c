#include "flash_app_handler.h"

Flash_Status_t Flash_GetConfig(Flash_Config_t *app_config)
{
	if(app_config == NULL)
	{
		return FLASH_APP_ERR;
	}
	app_config->first_boot = APP_CONFIG->first_boot;
	app_config->slot0_crc = APP_CONFIG->slot0_crc;
	app_config->slot1_crc = APP_CONFIG->slot1_crc;
	app_config->slot_status_flag = APP_CONFIG->slot_status_flag;
	app_config->curr_app_slot = APP_CONFIG->curr_app_slot;
	return FLASH_APP_OK;
}

Flash_Status_t Flash_WriteConfig(Flash_Config_t new_config)
{
	if(Flash_EraseSector(APP_CONFIG_FLASH_SECTOR, FLASH_VOLTAGE_RANGE_3) != FLASH_APP_OK)
	{
		printf("Error erasing app config flash\r\n");
		return FLASH_APP_ERR;
	}
	if(HAL_FLASH_Unlock() != HAL_OK)
		return FLASH_APP_ERR;
	//Stores the config in 32bit variable to write to Flash to keep things aligned
	uint32_t data = new_config.slot_status_flag << 24    	| 
					new_config.slot1_crc << 16 	| 
					new_config.slot0_crc << 8 	|
					new_config.first_boot;
	//writes data to flash memory
	if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_CONFIG_ADDR, data))
	{
		HAL_FLASH_Lock();
		printf("Error Writing New Config\r\n");
		return FLASH_APP_ERR;
	}
	//writes the app slot to flash config memory
	if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_CONFIG_ADDR + 0x04, new_config.curr_app_slot))
	{
		HAL_FLASH_Lock();
		printf("Error Writing New Config\r\n");
		return FLASH_APP_ERR;
	}
	HAL_FLASH_Lock();
	return FLASH_APP_OK;
}

Flash_Status_t Flash_EraseSector(uint32_t flash_sector, uint8_t flash_voltage_range)
{		
	if(flash_sector != APP_SLOT0_FLASH_SECTOR && flash_sector != APP_SLOT1_FLASH_SECTOR && flash_sector != APP_CONFIG_FLASH_SECTOR)
		return FLASH_APP_ERR;
	HAL_StatusTypeDef ret = HAL_FLASH_Unlock();
	if(ret != HAL_OK)
	{
		printf("Can't Unlock Flash\r\n");
		return FLASH_APP_ERR;
	}
	//initializes EraseInitTypeDef struct for erasing Flash Memory 
	FLASH_EraseInitTypeDef flash_erase_struct;
	uint32_t sector_err;
	flash_erase_struct.TypeErase = FLASH_TYPEERASE_SECTORS;
	flash_erase_struct.Sector = flash_sector;
	flash_erase_struct.NbSectors = 1;
	flash_erase_struct.VoltageRange = flash_voltage_range;
	ret = HAL_FLASHEx_Erase(&flash_erase_struct, &sector_err);
	if (ret != HAL_OK || sector_err != 0xFFFFFFFFU)
	{
		printf("Failed to Erase Flash\r\n");
		HAL_FLASH_Lock();
		return FLASH_APP_ERR;
	}
	return FLASH_APP_OK;
}

Flash_Status_t Flash_WriteData(uint32_t app_base_addr, uint8_t *data, uint16_t data_size)
{
	//keeps track of position of the flash memory as data is being written
	static uint16_t bytes_written = 0;
	if(data == NULL) return FLASH_APP_OK;
	HAL_StatusTypeDef ret = HAL_FLASH_Unlock();
	if (ret != HAL_OK)
		return FLASH_APP_ERR;
	//loops through data and writes to the Flash byte by byte
	for (uint16_t i = 0; i <  data_size; i++)
	{
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, app_base_addr + bytes_written, data[i]) != HAL_OK)
		{
			HAL_FLASH_Lock();
			return FLASH_APP_ERR;
		}
		bytes_written++;
	}

	ret = HAL_FLASH_Lock();
	if (ret != HAL_OK)
		return FLASH_APP_ERR;
	return FLASH_APP_OK;
}

Flash_Status_t Flash_CalculateCRC(uint32_t flash_addr, uint8_t *calculated_crc)
{
	if(!Flash_ValidFlashAppMem(flash_addr) || calculated_crc == NULL)
		return FLASH_APP_ERR;
	//enables RCC of CRC peripheral
	__HAL_RCC_CRC_CLK_ENABLE();
	//Resets the CRC
	CRC->CR |= CRC_CR_RESET;
	//Gets the flash address and calculates the end of the flash sector
	volatile uint32_t *addr = (volatile uint32_t *)flash_addr;
	volatile uint32_t *final_addr = (volatile uint32_t *)(flash_addr + APP_FLASH_SECTOR_SIZE);
	//loops through and sends flash data by WORD(32-bit) to CRC DR
	while(addr < final_addr)
	{
		CRC->DR = *addr++;
	}
	//Reads the calculated CRC value
	uint32_t crc = CRC->DR;
	//calculated CRC is stored as 8 bit value so we XOR the 4 bytes of CRC with each other
	*calculated_crc = ((crc >> 24) & 0xFF) ^ ((crc >> 16) & 0xFF) ^ ((crc >> 8) & 0xFF) ^ (crc & 0xFF);
	return FLASH_APP_OK;
}

uint8_t Flash_GetSector(uint32_t flash_addr)
{
	if(flash_addr == APP_SLOT1_ADDR)
		return APP_SLOT1_FLASH_SECTOR;
	else
		return APP_SLOT0_FLASH_SECTOR;
}

uint8_t Flash_ValidFlashAppMem(uint32_t flash_addr)
{
	if(flash_addr == APP_SLOT0_ADDR || flash_addr == APP_SLOT1_ADDR)
		return 1;
	else
		return 0;
}