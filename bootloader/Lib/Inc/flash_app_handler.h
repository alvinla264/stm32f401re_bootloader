#ifndef __FLASH_APP_HANDLER_H_
#define __FLASH_APP_HANDLER_H_

#include "main.h"
#include <stdint.h>
#include <stdio.h>

/*@ref APP_SLOT_ADDR*/
#define APP_SLOT0_ADDR 0x08020000U //SECTOR 5
#define APP_SLOT1_ADDR 0x08040000U //SECTOR 6

#define APP_SLOT0_FLASH_SECTOR FLASH_SECTOR_5
#define APP_SLOT1_FLASH_SECTOR FLASH_SECTOR_6
#define APP_CONFIG_FLASH_SECTOR FLASH_SECTOR_2
#define APP_CONFIG_ADDR 0x08008000U //SECTOR 2
#define APP_CONFIG ((volatile Flash_Config_t *)APP_CONFIG_ADDR)

#define APP_FLASH_SECTOR_SIZE 0x00020000

#define DEVICE_VOLTAGE_RANGE FLASH_VOLTAGE_RANGE_3

 /* @ref FIRST_BOOT_VALUES*/
#define FIRST_BOOT_FALSE 0x11U
#define FIRST_BOOT_TRUE  0xFFU	


typedef struct 
{
	volatile uint8_t first_boot; /* @ref FIRST_BOOT_VALUES*/
	volatile uint8_t slot0_crc;
	volatile uint8_t slot1_crc;
	volatile uint8_t slot_status_flag;
	volatile uint32_t curr_app_slot; /*@ref APP_SLOT_ADDR*/
} Flash_Config_t;

typedef enum 
{
	FLASH_APP_OK,
	FLASH_APP_ERR
} Flash_Status_t;

Flash_Status_t Flash_GetConfig(Flash_Config_t *app_config);
Flash_Status_t Flash_EraseSector(uint32_t flash_sector, uint8_t flash_voltage_range);
Flash_Status_t Flash_WriteConfig(Flash_Config_t new_config);
Flash_Status_t Flash_WriteData(uint32_t app_base_addr, uint8_t *data, uint16_t data_size);
Flash_Status_t Flash_CalculateCRC(uint32_t flash_addr, uint8_t *calculated_crc);
uint8_t Flash_GetSector(uint32_t flash_addr);
uint8_t Flash_ValidFlashAppMem(uint32_t flash_addr);
#endif //FLASH_APP_HANDLER_H_