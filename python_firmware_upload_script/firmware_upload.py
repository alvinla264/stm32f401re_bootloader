import serial
import serial.tools.list_ports
import threading
import time
import argparse

#TODO: Add stm32 ready for file transmission
#TODO: Have python code bring either app back to bootloader or just have python code start firmware update mode

ser = None
ser_lock = threading.Lock()
#Frame Format: [SOF(0x1A)][Payload Type(start sending data, data, end sending data)][Payload size(2 byte)][Payload][Checksum][EOF(0xA1)]
CHUNK_SIZE = 2048

OTA_SOF = 0x1A
OTA_EOF = 0xF3

OTA_DATA_TYPE_START_DATA = 0x31
OTA_DATA_TYPE_END_DATA = 0x32
OTA_DATA_TYPE_DATA = 0x33

OTA_ACK = 0xAA
OTA_NACK = 0xFF

OTA_NEW_FIRMWARE = 0x34


OTA_BOOTLOADER_UPLOAD_READY = 0xA2

MAX_RETRIES = 3
version = [0, 3]

DATA_TIMEOUT = 50
NEW_FIRMWARE_TIMEOUT = 15
START_DATA_FRAME_TIMEOUT = 5

endian_bytes_param = 'little'

ack_event = threading.Event()
nack_event = threading.Event()
upload_ready_event = threading.Event()
#Parse CLI argumenets
#Requires file path to binary file
def parse_arg():
	parser = argparse.ArgumentParser(description="STM32 Firmware Uploader")
	parser.add_argument(
		'--port', '-p',
		type=str,
		help='Serial COM port connected to STM32(e.g., COM3)'
	)
	parser.add_argument(
		'--file', '-f',
		type=str,
		required=True,
		help='Binary file path to upload to device(e.g., firmware.bin)'
	)
	parser.add_argument(
		'--baud', '-b',
		type=int,
		default=115200,
		help='Baud rate for serial communication (default: 115200)'
	)
	return parser.parse_args()
#detects STM32 UART COM Port
def detect_stm32():
	for port in serial.tools.list_ports.comports():
		if port.vid == 0x0483 and port.pid == 0x374B:
			return port.device
	return None
#only used if STM32 can't be detected
#prints out a list of com ports for user to select
def list_comports():
	available_ports = []
	for port in serial.tools.list_ports.comports():
		print(f'{port.device}: {port.description}')
		available_ports.append(port)
	return available_ports
#simple checksum calculation
def create_checksum(data):
	if not data:
		return 0x00
	if type(data) is int:
		return data
	checksum = data[0]
	for n in data[1:]:
		checksum= checksum ^ n
	return checksum
#thread function to read STM32 printf
#if no response is recogized then func echos response
def serial_read_thread():
	while True:
		with ser_lock:
			if ser != None and ser.in_waiting:
				try:
					x = ser.read_until().strip()
					if x == bytes([OTA_ACK]):
						ack_event.set()
					elif  x == bytes([OTA_NACK]):
						nack_event.set()
					elif x == bytes([OTA_BOOTLOADER_UPLOAD_READY]):
						upload_ready_event.set()
					else:
						print(f"STM32: {x.decode()}")
				except Exception as e:
					print(e)
		time.sleep(0.1)

reader_thread = threading.Thread(target=serial_read_thread, daemon=True)
reader_thread.start()

START_DATA_FRAME = bytes([OTA_SOF, OTA_DATA_TYPE_START_DATA] + list(0x01.to_bytes(2, endian_bytes_param))+ [OTA_DATA_TYPE_START_DATA] + [create_checksum([OTA_DATA_TYPE_START_DATA]), OTA_EOF])
END_DATA_FRAME = bytes([OTA_SOF, OTA_DATA_TYPE_END_DATA] + list(0x01.to_bytes(2, endian_bytes_param))+ [OTA_DATA_TYPE_END_DATA] + [create_checksum([OTA_DATA_TYPE_END_DATA]), OTA_EOF])

def main():
	global ser
	args = parse_arg()
	print(f"Bootloader Firmware Updater v{version[0]}.{version[1]}")
	port = None
	if args.port == None:
		port = detect_stm32()
		if port == None:
			list_of_ports = list_comports()
			is_valid_port = False
			while(not is_valid_port):
				port = input("Select a port: ")
				if port in list_of_ports:
					is_valid_port = True
	else:
		port = args.port
	if ser == None:
		ser = serial.Serial(port=port, baudrate=args.baud)
	x = args.file
	file_content = b'0'
	file_size = 0
	print(f'Firmware File Location: {x}')
	#reads in binary file
	with open(x, "rb") as file:
		file_content = file.read()
		file_size = len(file_content)
	print(f'File Size: {file_size/1000} KB')
	#calculates number of chunks
	num_of_chunk = int(file_size / CHUNK_SIZE)
	if (file_size % CHUNK_SIZE) > 0:
		num_of_chunk = num_of_chunk + 1
	print(f'Total Chunks({int(CHUNK_SIZE/1000)}KB each): {num_of_chunk}')
	print('Waiting for board to accept new firmware')
	#lets bootloader/application know there is a new firmware
	with ser_lock:
		ser.write(bytes([OTA_NEW_FIRMWARE]))
	start_time = time.time()
	while(not upload_ready_event.is_set()):
		if time.time() - start_time > NEW_FIRMWARE_TIMEOUT:
			print("Timeout occured resending data")
			with ser_lock:
				ser.write(bytes([OTA_NEW_FIRMWARE]))
			start_time = time.time()
	upload_ready_event.clear()
	print('Device ready sending firmware')
	time.sleep(0.3)
	with ser_lock:
		ser.write(START_DATA_FRAME)
	start_time = time.time()
	while( not ack_event.is_set()):
		if nack_event.is_set():
			print('Nack received resending data')
			with ser_lock:
				ser.write(START_DATA_FRAME)
			nack_event.clear()
		elif time.time() - start_time > START_DATA_FRAME_TIMEOUT:
			print("Timeout occured resending data")
			with ser_lock:
				ser.write(START_DATA_FRAME)
			start_time = time.time()
	ack_event.clear()
	num_of_retries = 0
	failed_transmission = 0
	n = 0
	while(len(file_content) > 0):
		chunked_data = list(file_content[0: CHUNK_SIZE])
		data_frame = bytes([OTA_SOF, OTA_DATA_TYPE_DATA] + list(len(chunked_data).to_bytes(2, endian_bytes_param))+ chunked_data + [create_checksum(chunked_data), OTA_EOF])
		print(f"Sending Chunk: {n + 1}")
		with ser_lock:
			ser.write(data_frame)
		start_time = time.time()
		while True:
			if ack_event.is_set():
				n = n + 1
				file_content = file_content[CHUNK_SIZE: ]
				ack_event.clear()
				num_of_retries = 0
				break
			elif nack_event.is_set():
				print('NACK received retransmitting frame')
				num_of_retries = num_of_retries + 1
				nack_event.clear()
				break
			elif time.time() - start_time > DATA_TIMEOUT:
				print('Timeout waiting for ACK/NACK')
				print('Retransmitting frame')
				start_time = time.time()
				num_of_retries = num_of_retries + 1
				break
		if num_of_retries > MAX_RETRIES:
			failed_transmission = 1
			break
			
				
	if failed_transmission:
		print("Max Retransmission Reached.")
		print("Failed to transmit whole file")
	elif num_of_chunk != n:
		print('Sent Chunks does not equal calculated chunks')
		print('Transmission corrupted')
	else:
		with ser_lock:
			ser.write(END_DATA_FRAME)
		start_time = time.time()
		while( not ack_event.is_set()):
			if nack_event.is_set():
				print('Nack received resending end data frame')
				with ser_lock:
					ser.write(END_DATA_FRAME)
				nack_event.clear()
			elif time.time() - start_time > 5:
				#send_data_frame(OTA_DATA_TYPE_END_DATA, 1, OTA_DATA_TYPE_END_DATA)
				with ser_lock:
					ser.write(END_DATA_FRAME)
				start_time = time.time()
		ack_event.clear()
		print('Sucessfully sent firmware to device')
	user_input = ''
	while(user_input != 'q'):
		user_input = input("Type q to quit or r to restart \n\n")
		if user_input == 'r':
			print("Please restart stm32 board")
			main()
			return


if __name__ == '__main__':
	main()