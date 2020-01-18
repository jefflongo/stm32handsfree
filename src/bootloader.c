#ifdef _WIN32
#include "ftd2xx.h"
#elif __linux__
#include "ftdi.h"
#include <libusb-1.0/libusb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// FT232R write logic:
// Upper nybble: pin directions (0 == input, 1 == output)
// Bit 7 Bit 6 Bit 5 Bit 4
// CBUS3 CBUS2 CBUS1 CBUS0
//
// Lower nybble: pin states (0 == low, 1 == high)
// Bit 3 Bit 2 Bit 1 Bit 0
// CBUS3 CBUS2 CBUS1 CBUS0
//
// Configuration:
// CBUS0 -> unused
// CBUS1 -> unused
// CBUS2 -> BOOT0
// CBUS3 -> RESET

#define TRANSITION_DELAY 2000 // microseconds

#ifdef _WIN32
#define BITMODE_CBUS 0x20

static FT_HANDLE ft_handle;

static inline int dev_open(void) {
    return FT_Open(0, &ft_handle);
}

static inline int dev_close(void) {
    return FT_Close(ft_handle);
}

static inline int dev_write(unsigned char data) {
    return FT_SetBitMode(ft_handle, data, BITMODE_CBUS);
}
#elif __linux__
#define FT232_VID 0x0403
#define FT232_PID 0x6001
#define FT_OK 0

static struct ftdi_context ft_handle;

static inline int dev_open(void) {
    return ftdi_usb_open(&ft_handle, FT232_VID, FT232_PID);
}

static inline int dev_close(void) {
    return ftdi_usb_close(&ft_handle);
}

static inline int dev_write(unsigned char data) {
    return ftdi_set_bitmode(&ft_handle, data, BITMODE_CBUS);
}
#else
#error OS not supported
#endif

static int find_device(char** dev) {
    int status = dev_open();
    if (status != FT_OK) return status;

#ifdef _WIN32
#define COM_PORT_MAX_LENGTH 7 // COMXYZ + null terminator
    LONG port;
    status = FT_GetComPortNumber(ft_handle, &port);
    if (status != FT_OK) return status;
    if (port == -1) return FT_DEVICE_NOT_FOUND;
    *dev = (char*)malloc(COM_PORT_MAX_LENGTH * sizeof(char));
    snprintf(*dev, sizeof(*dev), "COM%d", (int)port);
#elif __linux__
    *dev = "/dev/ttyUSB0";
#endif
    status = dev_close();
    return status;
}

static int enter_bootloader(void) {
    int status = dev_open();
    if (status != FT_OK) return status;
    // BOOT0: 0
    // RESET: 0
    if (status == FT_OK) status = dev_write(0xC3);
    usleep(TRANSITION_DELAY);
    // BOOT0: 1
    // RESET: 0
    if (status == FT_OK) status = dev_write(0xC7);
    usleep(TRANSITION_DELAY);
    // BOOT0: 1
    // RESET: 1
    if (status == FT_OK) status = dev_write(0x4F);
    status = dev_close();

    return status;
}

static int exit_bootloader(void) {
    int status = dev_open();
    if (status != FT_OK) return status;
    // BOOT0: 0
    // RESET: 1
    if (status == FT_OK) status = dev_write(0x4B);
    usleep(TRANSITION_DELAY);
    // BOOT0: 0
    // RESET: 0
    if (status == FT_OK) status = dev_write(0xC3);
    usleep(TRANSITION_DELAY);
    // BOOT0: 0
    // RESET: 1
    if (status == FT_OK) status = dev_write(0x4B);
    // BOOT0 -> INPUT
    // RESET -> INPUT
    if (status == FT_OK) status = dev_write(0x0F);
    status = dev_close();

    return status;
}

#ifdef _WIN32
#define FLASH_PROGRAM "STM32_Programmer_CLI.exe"
#elif __linux__
#define FLASH_PROGRAM "STM32_Programmer_CLI"
#endif
#define FLASH_CONNECT_ARG " -c port="
#define FLASH_WRITE_ARG " -w "
#define FLASH_WRITE_ADDR " 0x08000000"
#define FLASH_VERIFY_ARG " -v"

char* parse(char* dev, char* binary_path) {
    int command_size = strlen(FLASH_PROGRAM FLASH_CONNECT_ARG FLASH_WRITE_ARG
                                FLASH_WRITE_ADDR FLASH_VERIFY_ARG) +
                       strlen(dev) + strlen(binary_path) +
                       1; // Plus 1 for null terminator
    char* command = (char*)malloc(command_size * sizeof(char));
    snprintf(
      command,
      command_size,
      "%s%s%s%s%s%s%s",
      FLASH_PROGRAM,
      FLASH_CONNECT_ARG,
      dev,
      FLASH_WRITE_ARG,
      binary_path,
      FLASH_WRITE_ADDR,
      FLASH_VERIFY_ARG);

    return command;
}

int main(int argc, char** argv) {
    char* dev;
    char* command;

    if (argc != 2) {
        fprintf(stderr, "usage: <path/to/binary>");
        return -1;
    }
    if (find_device(&dev) != FT_OK) {
        fprintf(stderr, "Failed to find device");
        return -1;
    }
    if (enter_bootloader() != FT_OK) {
        fprintf(stderr, "Failed to enter bootloader mode");
        return -1;
    }

    command = parse(dev, argv[1]);
    system(command);
    //free(dev);
    free(command);

    if (exit_bootloader() != FT_OK) {
        fprintf(stderr, "Failed to exit bootloader mode");
        return -1;
    }

    return 0;
}