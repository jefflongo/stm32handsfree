#include "ftd2xx.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG_LEVEL 0

// FT232R write logic:
// Upper nybble: pin directions (0 == input, 1 == output)
// Bit 7 Bit 6 Bit 5 Bit 4
// CBUS4 CBUS3 CBUS2 CBUS1
//
// Lower nybble: pin states (0 == low, 1 == high)
// Bit 3 Bit 2 Bit 1 Bit 0
// CBUS4 CBUS3 CBUS2 CBUS1
//
// Configuration:
// CBUS0 -> unused
// CBUS1 -> unused
// CBUS2 -> BOOT0
// CBUS3 -> RESET
//
// To compile:
// gcc -o bootload bootloader.c -L. -lftd2xx

#define FT_MODE_ENABLE 0x20
#define FT_MODE_RESET 0x00

static FT_HANDLE ft_handle;

static inline FT_STATUS ft_write(UCHAR data) {
    return FT_SetBitMode(ft_handle, data, FT_MODE_ENABLE);
}

static FT_STATUS enter_bootloader(void) {
    FT_STATUS ok = FT_Open(0, &ft_handle);
    if (ok != FT_OK) {
#if DEBUG_LEVEL >= 2
        printf("Failed to open serial port.\n");
#endif
        return ok;
    }
    // BOOT0: 0
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC3);
    usleep(2000);
    // BOOT0: 1
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC7);
    usleep(2000);
    // BOOT0: 1
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0xCF);
    ok = FT_Close(ft_handle);
#if DEBUG_LEVEL >= 2
    if (ok != FT_OK) printf("Failed to close serial port.\n");
#endif

    return ok;
}

static FT_STATUS exit_bootloader(void) {
    FT_STATUS ok = FT_Open(0, &ft_handle);
    if (ok != FT_OK) {
#if DEBUG_LEVEL >= 2
        printf("Failed to open serial port.\n");
#endif
        return ok;
    }
    // BOOT0: 0
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0xCB);
    usleep(2000);
    // BOOT0: 0
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC3);
    usleep(2000);
    // BOOT0: 0
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0xCB);
    // BOOT0 -> INPUT
    // RESET -> INPUT
    if (ok == FT_OK) ok = ft_write(0x0F);
    ok = FT_Close(ft_handle);
#if DEBUG_LEVEL >= 2
    if (ok != FT_OK) printf("Failed to close serial port.\n");
#endif

    return ok;
}

#define FLASH_PROGRAM "STM32_programmer_cli.exe"
#define FLASH_CONNECT_ARG " -c port="
#define FLASH_WRITE_ARG " -w "
#define FLASH_WRITE_ADDR " 0x08000000"
#define FLASH_VERIFY_ARG " -v"

int main(int argc, char** argv) {
    // Parse
    if (argc != 3) {
        printf("usage: <serial port> <path/to/binary>");
        return -1;
    }
    int command_size = strlen(FLASH_PROGRAM) + strlen(FLASH_CONNECT_ARG) +
                       strlen(FLASH_WRITE_ARG) + strlen(FLASH_WRITE_ADDR) +
                       strlen(FLASH_VERIFY_ARG) + strlen(argv[1]) +
                       strlen(argv[2]) + 1; // Plus 1 for null terminator
    char* command = (char*)malloc(command_size);
    snprintf(
      command,
      command_size,
      "%s%s%s%s%s%s%s",
      FLASH_PROGRAM,
      FLASH_CONNECT_ARG,
      argv[1],
      FLASH_WRITE_ARG,
      argv[2],
      FLASH_WRITE_ADDR,
      FLASH_VERIFY_ARG);

    FT_STATUS status = enter_bootloader();
#if DEBUG_LEVEL >= 1
    if (status != FT_OK) printf("Failed to enter bootloader.\n");
#endif

    // Program target
    system(command);
    free(command);

    status = exit_bootloader();
#if DEBUG_LEVEL >= 1
    if (status != FT_OK) printf("Failed to exit bootloader.\n");
#endif

    return status == FT_OK ? 0 : -1;
}