#include "ota_update.h"

extern UART_HandleTypeDef huart2;
/**
 * @brief calculates a XOR checksum and checks it with the payload being sent from uploader
 * 
 * @param df DataFrame struct received from uploader
 * @return OTA_Status_t 
 */
static OTA_Status_t checksum_verify(OTA_DataFrame_t *df)
{
	uint8_t checksum = df->data[0];
	for (uint16_t i = 1; i < df->data_size; i++)
	{
		checksum ^= df->data[i];
	}
	return (checksum != df->crc) ? OTA_ERR : OTA_OK;
}
/**
 * @brief Sends one byte over through UART2
 * 
 * @param reponse Byte to send over
 */
static void send_byte_response(uint8_t reponse) { 
	HAL_UART_Transmit(&huart2, &reponse, 1, HAL_MAX_DELAY); 
	printf("\r\n");
}
/**
 * @brief Get the data frame from uploader via UART2
 * 
 * @param df pointer to data frame to store the received value
 * @return OTA_Status_t 
 */
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
		//looks for the SOF byte to sync messages
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
		//Gets data type byte
		ret = HAL_UART_Receive(&huart2, &df->data_type, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//gets the data size(2 bytes)
		ret = HAL_UART_Receive(&huart2, (uint8_t *)&df->data_size, 2, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//printf("Size: %x\r\n", df->data_size);
		//gets the data based on the data size
		ret = HAL_UART_Receive(&huart2, df->data, df->data_size, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//gets the crc calculated from the uploader
		ret = HAL_UART_Receive(&huart2, &df->crc, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//gets eof for sync
		ret = HAL_UART_Receive(&huart2, &df->eof, 1, HAL_MAX_DELAY);
		if (ret != HAL_OK)
		{
			error_detected = 1;
		}
		//checks if there has been any UART receive errors 
		//or if eof has been received
		//also checks if checksum is valid
		//Sends NACK if the data frame is not valid and then retries
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
			//Sends ACK of Data frame is valid and the
			//data type is not START DATA
			//This is because we want to ACK after
			//we erase the flash
			//if we erase the flash after we ack there will be data
			//loss
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
	//checks to make sure app_addr is a valid app SLOT#
	if(!Flash_ValidFlashAppMem(app_addr))
	{
		printf("Invalid Flash Address\r\n");
		return OTA_ERR;
	}
	OTA_DataFrame_t df = {0};
	printf("Waiting for firmware\r\n");
	//Lets the uploader know the firmware is ready to be received
	send_byte_response(OTA_BOOTLOADER_UPLOAD_READY);
	OTA_Status_t ret;
	//Checks if the data type is start data
	//anything else we discard
	while(df.data_type != OTA_DATA_TYPE_START_DATA)
	{
		ret = get_data_frame(&df);
		if (ret != OTA_OK)
		{
			printf("Error Obtaining Data Frame\r\n");
			return OTA_ERR;
		}
	}
	//erase the flash sector to store the data
	printf("Download starting erasing flash\r\n");
	if(Flash_EraseSector(Flash_GetSector(app_addr), DEVICE_VOLTAGE_RANGE) != FLASH_APP_OK)
	{
		printf("Error erasing flash\r\n");
		return OTA_ERR;
	}
	//we ack after erasing flash since there is a short delay
	//when erasing flash and we get data loss because the uploader
	//sends the data immediatly after receving ack
	send_byte_response(OTA_ACK);
	//loops through until we receive the END DATA data type
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
		//writes firmware to flash memory
		if (Flash_WriteData(app_addr, df.data, df.data_size) != FLASH_APP_OK)
		{
			printf("Error Writing to Flash\r\n");
			return OTA_ERR;
		}
	}
	printf("Finished writing new firmware!\r\n");
	return OTA_OK;
}
