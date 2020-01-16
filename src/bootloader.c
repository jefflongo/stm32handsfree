#include "ftd2xx.h"

#include <stdio.h>
#include <stdlib.h>
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

#define FT_MODE_ENABLE 0x20
#define FT_MODE_RESET 0x00

static FT_HANDLE ft_handle;

static inline FT_STATUS ft_write(UCHAR data) {
    return FT_SetBitMode(ft_handle, data, FT_MODE_ENABLE);
}

static FT_STATUS enter_bootloader(LPLONG port_number_ptr) {
    FT_STATUS ok = FT_Open(0, &ft_handle);
    if (ok != FT_OK) return ok;
    // Find serial port
    if (ok == FT_OK) ok = FT_GetComPortNumber(ft_handle, port_number_ptr);

    // BOOT0: 0
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC3);
    usleep(TRANSITION_DELAY);
    // BOOT0: 1
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC7);
    usleep(TRANSITION_DELAY);
    // BOOT0: 1
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0x4F);
    ok = FT_Close(ft_handle);

    return ok;
}

static FT_STATUS exit_bootloader(void) {
    FT_STATUS ok = FT_Open(0, &ft_handle);
    if (ok != FT_OK) return ok;
    // BOOT0: 0
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0x4B);
    usleep(TRANSITION_DELAY);
    // BOOT0: 0
    // RESET: 0
    if (ok == FT_OK) ok = ft_write(0xC3);
    usleep(TRANSITION_DELAY);
    // BOOT0: 0
    // RESET: 1
    if (ok == FT_OK) ok = ft_write(0x4B);
    // BOOT0 -> INPUT
    // RESET -> INPUT
    if (ok == FT_OK) ok = ft_write(0x0F);
    ok = FT_Close(ft_handle);

    return ok;
}

#define FLASH_PROGRAM "STM32_programmer_cli.exe"
#define FLASH_CONNECT_ARG " -c port="
#define FLASH_WRITE_ARG " -w "
#define FLASH_WRITE_ADDR " 0x08000000"
#define FLASH_VERIFY_ARG " -v"

char* parse(LONG port_number, char* binary_path) {
    char port[5];
    snprintf(port, sizeof(port), "COM%d", (int)port_number);
    int command_size = strlen(FLASH_PROGRAM FLASH_CONNECT_ARG FLASH_WRITE_ARG
                                FLASH_WRITE_ADDR FLASH_VERIFY_ARG) +
                       strlen(port) + strlen(binary_path) +
                       1; // Plus 1 for null terminator
    char* command = (char*)malloc(command_size);
    snprintf(
      command,
      command_size,
      "%s%s%s%s%s%s%s",
      FLASH_PROGRAM,
      FLASH_CONNECT_ARG,
      port,
      FLASH_WRITE_ARG,
      binary_path,
      FLASH_WRITE_ADDR,
      FLASH_VERIFY_ARG);

    return command;
}

int main(int argc, char** argv) {
    // Parse and build STM32CubeProgrammer command
    if (argc != 2) {
        printf("usage: <path/to/binary>");
        return -1;
    }

    // Program target
    FT_STATUS status;
    LONG port_number;
    status = enter_bootloader(&port_number);
    if (status != FT_OK) {
        printf("Failed to enter bootloader mode.\n");
        return -1;
    }
    if (port_number == -1) {
        printf("No device detected.\n");
        return -1;
    }
    char* command = parse(port_number, argv[1]);
    system(command);
    free(command);
    status = exit_bootloader();

    return status == FT_OK ? 0 : -1;
}