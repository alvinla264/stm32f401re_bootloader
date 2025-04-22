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
/**
 * @brief Gets the application config stored in Flash Memory
 * 
 * @param app_config pointer to Flash_Config_t struct to store the flash config
 * @return Flash_Status_t 
 */
Flash_Status_t Flash_GetConfig(Flash_Config_t *app_config);

/**
 * @brief Erases a sector of the Flash Memory
 * 
 * @param flash_sector Flash sector to erase(Sector 2, 5, or 6 only)
 * @param flash_voltage_range MCU voltage range
 * @return Flash_Status_t 
 */
Flash_Status_t Flash_EraseSector(uint32_t flash_sector, uint8_t flash_voltage_range);
/**
 * @brief Writes new Application config to the Flash
 * 
 * @param new_config new Application config to write to Flash
 * @return Flash_Status_t 
 */
Flash_Status_t Flash_WriteConfig(Flash_Config_t new_config);
/**
 * @brief Writes firmware data to Flash
 * 
 * @param app_base_addr [ @ref APP_SLOT_ADDR ] Base address(0x0802 0000(Slot0, Sector 5) or 0x0804 0000(Slot1, Sector 7))
 * @param data pointer to data to write to Flash
 * @param data_size size of data pointer
 * @return Flash_Status_t 
 */
Flash_Status_t Flash_WriteData(uint32_t app_base_addr, uint8_t *data, uint16_t data_size);
/**
 * @brief Calcuates CRC for Slot0(Flash Sector 5) or Slot1 (Flash Sector 6). Only
 * calculates 128KB since Sector 5 and Sector 6 are 128KB
 * 
 * @param flash_addr Address of Slot0 or Slot1
 * @param calculated_crc pointer to variable to store the calculated crc value
 * @return Flash_Status_t 
 */
Flash_Status_t Flash_CalculateCRC(uint32_t flash_addr, uint8_t *calculated_crc);
/**
 * @brief Gets the HAL Flash sector macro based on the given address. 
 * 
 * @param flash_addr [ @ref APP_SLOT_ADDR] Flash Memory Address to get sector (Sector 5 and Sector 6 only)
 * @return uint8_t Returns the HAL Flash sector macro (Sector 5 and Sector 6 only)
 */
uint8_t Flash_GetSector(uint32_t flash_addr);
/**
 * @brief Checks whether the flash address is a valid Application Slot#
 * 
 * @param flash_addr [ @ref APP_SLOT_ADDR ]
 * @return uint8_t returns 1 if it is a valid application slot# or 0 if not
 */
uint8_t Flash_ValidFlashAppMem(uint32_t flash_addr);
#endif //FLASH_APP_HANDLER_H_