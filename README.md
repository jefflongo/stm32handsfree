# stm32handsfree

Hands-free USB flashing tool for STM32.

This tool utilizes FTDI's FT232R UART-USB bridge for automated BOOT0/NRST control. When designing your circuit, connect BOOT0 to CBUS2 alongside a pull-down resistor, and NRST to CBUS3 alongside a pull-up resistor.

To use this software, install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) and add it to your system PATH. The program can be built with the provided Makefile. To flash your microcontroller, run the executable with the path to the program binary as the argument.