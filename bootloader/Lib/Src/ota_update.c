#include "ota_update.h"

extern UART_HandleTypeDef huart2;

static OTA_Status_t checksum_verify(OTA_DataFrame_t *df)
{
	uint8_t checksum = df->data[0];
	for (uint16_t i = 1; i < df->data_size; i++)
	{
		checksum ^= df->data[i];
	}
	return (checksum != df->crc) ? OTA_ERR : OTA_OK;
}

static void send_byte_response(uint8_t reponse) { 
	HAL_UART_Transmit(&huart2, &reponse, 1, HAL_MAX_DELAY); 
	printf("\r\n");
}

static OTA_Status_t get_data_frame(OTA_DataFrame_t *df)
{
	static uint8_t num_of_retries = 0;
	HAL_StatusTypeDef ret;
	uint8_t error_detected = 0;
	uint8_t num_of_sof_retries = 0;
	uint8_t byte = 0;
	while (num_of_retries <= MAX_RETRIES)
	{
		error_detected = 0;
		while(num_of_sof_retries <= MAX_SOF_RETRIES)
		{
			ret = HAL_UART_Receive(&huart2, &byte, 1, HAL_MAX_DELAY);
			if (ret != HAL_OK)
			{
				error_detected = 1;
			}
			if(byte == OTA_SOF)
			{
				df->sof = byte;
				break;
			}
		}
		ret = HAL_UART_Receive(&huart2, &df->data_type, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		ret = HAL_UART_Receive(&huart2, (uint8_t *)&df->data_size, 2, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//printf("Size: %x\r\n", df->data_size);
		ret = HAL_UART_Receive(&huart2, df->data, df->data_size, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		ret = HAL_UART_Receive(&huart2, &df->crc, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		ret = HAL_UART_Receive(&huart2, &df->eof, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		if (df->eof != OTA_EOF ||  checksum_verify(df) != OTA_OK || error_detected)
		{
			num_of_retries++;
			if(df->eof != OTA_EOF)
				printf("EOF error\r\n");
			if(checksum_verify(df) != OTA_OK)
				printf("Checksum error\r\n");
			if(error_detected)
				printf("Transimission error detected\r\n");
			send_byte_response(OTA_NACK);
		}
		else
		{
			//printf("Valid Frame\r\n");
			if(df->data_type != OTA_DATA_TYPE_START_DATA)
				send_byte_response(OTA_ACK);
			break;
		}
	}
	if (num_of_retries > MAX_RETRIES)
		return OTA_ERR;
	return OTA_OK;
}

OTA_Status_t ota_download_and_flash(uint32_t app_addr)
{
	if(!Flash_ValidFlashAppMem(app_addr))
	{
		printf("Invalid Flash Address\r\n");
		return OTA_ERR;
	}
	OTA_DataFrame_t df = {0};
	printf("Waiting for firmware\r\n");
	send_byte_response(OTA_BOOTLOADER_UPLOAD_READY);
	OTA_Status_t ret;
	while(df.data_type != OTA_DATA_TYPE_START_DATA)
	{
		ret = get_data_frame(&df);
		if (ret != OTA_OK)
		{
			printf("Error Obtaining Data Frame\r\n");
			return OTA_ERR;
		}
	}
	printf("Download starting erasing flash\r\n");
	if(Flash_EraseSector(Flash_GetSector(app_addr), DEVICE_VOLTAGE_RANGE) != FLASH_APP_OK)
	{
		printf("Error erasing flash\r\n");
		return OTA_ERR;
	}
	send_byte_response(OTA_ACK);
	while (1)
	{
		ret = get_data_frame(&df);
		if (ret != OTA_OK)
		{
			return OTA_ERR;
		}
		if (df.data_type == OTA_DATA_TYPE_END_DATA)
		{
			break;
		}
		//printf("%x %x %d %x %x\r\n", df.sof, df.data_type, df.data_size, df.crc, df.eof);
		if (Flash_WriteData(app_addr, df.data, df.data_size) != FLASH_APP_OK)
		{
			printf("Error Writing to Flash\r\n");
			return OTA_ERR;
		}
	}
	printf("Finished writing new firmware!\r\n");
	return OTA_OK;
}
