# 🔧 STM32F401RE UART Bootloader

A custom bootloader for the STM32F401RE (Nucleo board) that enables firmware updates over UART.

---

## 🚀 Features

- Receives new firmware over UART
- Writes firmware to flash memory
- Jumps to application after successful update
- Dual application slot(Main application slot and Backup slot)
- Tamper Detection
- Compact and efficient, written in bare-metal C
- Works with STM32CubeIDE or Makefile-based toolchains

---

## 🛠️ Requirements

- STM32F401RE Nucleo board
- Onboard Nucleo ST-Link(Or UART2 and USB-Serial Connection)
- CMake
- STM32 Programmer CLI
- Python 3.x (if using host upload script)

---

## 🧱 Memory Layout

| Region                  | Start Address  | Size      |
|-------------------------|----------------|-----------|
| Bootloader              | `0x08000000`   | ~16 KB    |
| Main Application Slot   | `0x08020000`   | ~128 KB   |
| Backup Application Slot | `0x08040000`   | ~128 KB   |

> Make sure your application linker script starts at `0x08020000` and allocate 128KB for Flash.
> Offset Vector Table (in system_stm32f4xx.c) by 0x00020000U

---

## ⚙️ Building
- cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE:FILEPATH=cmake/gcc-arm-none-eabi.cmake -S . -B ./build/Debug -G Ninja
- cmake --build /build/Debug

## 🔦 Flashing Bootloader
- STM32_Programmer_CLI --connect port=swd --download build\Debug\bootloader.elf -hardRst -rst --start

## ⏳ In Progress
- Better Error Handling instead of using ErrorHandling()

## 📝 TODO
- Implement OTA firmware via Bluetooth
- Support encrypted firmware uploads