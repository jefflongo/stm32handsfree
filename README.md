# stm32handsfree

Cross-platform hands-free USB flashing tool for STM32.

This tool utilizes FTDI's FT232R UART-USB bridge for automated BOOT0/NRST control. When designing your circuit, connect BOOT0 to CBUS2 alongside a pull-down resistor, and NRST to CBUS3 alongside a pull-up resistor.

To use this software, install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) and add it to your system PATH. The program can be built with the provided Makefile. To flash your microcontroller, run the executable with the path to the program binary as the argument.

# Notes for Linux
- This software has the following library dependencies: libusb, libudev, and a custom build of libftdi (provided as included zip) containing a bug fix critical to the operation of this software. To build this custom version of libftdi, unzip it and follow the instructions in its README to install into the root directory of this project.
- This software requires access to the USB ports. Therefore, the executable must either be ran as `sudo` (with STM32CubeProgrammer on the root PATH), or the current user must be added to the `dialout` group. This can be performed with the command `usermod -a -G dialout <user>`.
