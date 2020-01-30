#ifdef _WIN32
#include "ftd2xx.h"
#elif __linux__
#include "libftdi1/ftdi.h"

#include <libudev.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#ifdef _WIN32
#define BITMODE_CBUS 0x20

static FT_HANDLE ftdi;

static inline int dev_open(void) {
    return FT_Open(0, &ftdi);
}

static inline int dev_close(void) {
    return FT_Close(ftdi);
}

static inline int dev_write(unsigned char data) {
    return FT_SetBitMode(ftdi, data, BITMODE_CBUS);
}
#elif __linux__
#define FT232_VID 0x0403
#define FT232_PID 0x6001
#define FT_OK 0
#define FT_DEVICE_NOT_FOUND 2

static struct ftdi_context* ftdi;

static inline int dev_open(void) {
    return ftdi_usb_open(ftdi, FT232_VID, FT232_PID);
}

static inline int dev_close(void) {
    return ftdi_usb_close(ftdi);
}

static inline int dev_write(unsigned char data) {
    return ftdi_set_bitmode(ftdi, data, BITMODE_CBUS);
}
#else
#error OS not supported
#endif

static int find_device(char** loc) {
    int status;
#ifdef _WIN32
#define COM_PORT_MAX_LENGTH 7 // COMXYZ + null terminator
    status = dev_open();
    if (status != FT_OK) return status;
    LONG port;
    status = FT_GetComPortNumber(ftdi, &port);
    if (status == FT_OK && port == -1) status = FT_DEVICE_NOT_FOUND;
    if (status == FT_OK) *loc = (char*)malloc(COM_PORT_MAX_LENGTH * sizeof(char));
    if (status == FT_OK) snprintf(*loc, sizeof(*loc), "COM%d", (int)port);
    if (status == FT_OK)
        status = dev_close();
    else
        dev_close();
#elif __linux__
    status = FT_DEVICE_NOT_FOUND;

    const char* path;
    int vid, pid;
    struct udev* udev;
    struct udev_enumerate* enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device* dev;

    // Enumerate devices in tty subsystem
    udev = udev_new();
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    // Iterate and create udev device for each entry
    udev_list_entry_foreach(dev_list_entry, devices) {
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        // Get device path
        path = udev_device_get_devnode(dev);
        // Filter for ttyUSB devices
        if (strstr(path, "USB")) {
            // Retrieve USB device information
            dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
            vid = (int)strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16);
            pid = (int)strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16);
            // Match VID/PID to ttyUSB path
            if (vid == FT232_VID && pid == FT232_PID) {
                *loc = (char*)malloc((strlen(path) + 1) * sizeof(char));
                *loc = (char*)path;
                status = FT_OK;
                udev_device_unref(dev);
                break;
            }
        }
        udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
#endif
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
    if (status == FT_OK)
        status = dev_close();
    else
        dev_close();

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
    if (status == FT_OK)
        status = dev_close();
    else
        dev_close();

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
    int command_size =
      strlen(FLASH_PROGRAM FLASH_CONNECT_ARG FLASH_WRITE_ARG FLASH_WRITE_ADDR FLASH_VERIFY_ARG) +
      strlen(dev) + strlen(binary_path) + 1; // Plus 1 for null terminator
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

#ifdef __linux__
    ftdi = ftdi_new();
    ftdi->module_detach_mode = AUTO_DETACH_REATACH_SIO_MODULE;
#endif

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

#ifdef __linux__
    ftdi_deinit(ftdi);
#endif

    return 0;
}