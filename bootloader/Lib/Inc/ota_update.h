#ifndef OTA_UPDATE_H_
#define OTA_UPDATE_H_

#include "main.h"
#include <stdio.h>
#include <string.h>
#include "flash_app_handler.h"

#define OTA_SOF 0x1A
#define OTA_EOF 0xF3

#define OTA_ACK 0xAA
#define OTA_NACK 0xFF

#define OTA_BOOTLOADER_UPLOAD_READY 0xA2

#define MAX_DATA_SIZE 2048

#define MAX_RETRIES 5

#define MAX_SOF_RETRIES 10


//6 + max_data_point = 512 + 6 = 518
/*
Data Frame 
[SOF(1 byte)] [Data Type(1 byte)] [Data Size(2 bytes)] [Data(Data Size bytes)] [CRC(1 byte)] [EOF(1 byte)]
index: [0] [1] [2-3] [4 - 3 + size] [4 + size] [5 + size] 

[1a(0)] [31(1)] [1(2-3)] [0(4)] [0(5)] [F3(7)]
[1a(0)] [31(1)] [04(2-3)] [11(4), 22,(5), 33(6), 44(7)] [CRC(8)] [F3(9)]
*/
typedef enum 
{
	OTA_DATA_TYPE_START_DATA = 0x31,
	OTA_DATA_TYPE_END_DATA = 0x32,
	OTA_DATA_TYPE_DATA = 0x33
}OTA_Data_Type_t;

typedef enum
{
	OTA_OK,
	OTA_ERR
} OTA_Status_t;

typedef struct 
{
	uint8_t sof;
	uint8_t data_type;
	uint16_t data_size;
	uint8_t data[MAX_DATA_SIZE];
	uint8_t crc;
	uint8_t eof;
}OTA_DataFrame_t;

OTA_Status_t ota_download_and_flash(uint32_t app_addr);

#endif //eof OTA_UPDATE_H_