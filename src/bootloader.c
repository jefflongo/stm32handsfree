#include "ftd2xx.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// FT232R write logic:
//
// CBUS bits
// 3210 3210
// xxxx xxxx
// |    |------ Output state:  0 -> low,   1 -> high
// |----------- Pin direction: 0 -> input, 1 -> output
//
// Configuration:
// CBUS0 -> unused
// CBUS1 -> unused
// CBUS2 -> BOOT0
// CBUS3 -> RESET

#define TRANSITION_DELAY 2000 // microseconds

static FT_HANDLE ft_handle;

static inline FT_STATUS dev_open(void) {
    return FT_Open(0, &ft_handle);
}

static inline FT_STATUS dev_close(void) {
    return FT_Close(ft_handle);
}

#define BITMODE_CBUS 0x20

static inline FT_STATUS dev_write(UCHAR data) {
    return FT_SetBitMode(ft_handle, data, BITMODE_CBUS);
}

#define COM_PORT_MAX_LENGTH 7 // COMXYZ + null terminator

static FT_STATUS find_device(char** dev) {
    FT_STATUS status = dev_open();
    if (status != FT_OK) return status;

    LONG port;
    status = FT_GetComPortNumber(ft_handle, &port);
    if (status != FT_OK) return status;
    if (port == -1) return FT_DEVICE_NOT_FOUND;
    *dev = (char*)malloc(COM_PORT_MAX_LENGTH * sizeof(char));
    snprintf(*dev, sizeof(*dev), "COM%d", (int)port);

    status = dev_close();

    return status;
}

static FT_STATUS enter_bootloader(void) {
    FT_STATUS status = dev_open();
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

static FT_STATUS exit_bootloader(void) {
    FT_STATUS status = dev_open();
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

#define FLASH_PROGRAM "STM32_programmer_cli.exe"
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
        fprintf(stderr, "usage: <path/to/binary>\n");
        return -1;
    }
    if (find_device(&dev) != FT_OK) {
        fprintf(stderr, "Failed to find device\n");
        return -1;
    }
    if (enter_bootloader() != FT_OK) {
        fprintf(stderr, "Failed to enter bootloader mode\n");
        return -1;
    }

    command = parse(dev, argv[1]);
    system(command);
    free(dev);
    free(command);

    if (exit_bootloader() != FT_OK) {
        fprintf(stderr, "Failed to exit bootloader mode\n");
        return -1;
    }

    return 0;
}