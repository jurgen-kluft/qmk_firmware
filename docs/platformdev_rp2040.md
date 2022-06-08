# Raspberry Pi RP2040

The following table shows the current driver status for peripherals on RP2040 MCUs:

| System           | Support                                        |
| ---------------- | ---------------------------------------------- |
| ADC driver       | Support planned (no ETA)                       |
| Audio            | Support planned (no ETA)                       |
| I2C driver       | :heavy_check_mark:                             |
| SPI driver       | :heavy_check_mark:                             |
| WS2812 driver    | :heavy_check_mark: using `PIO` driver          |
| External EEPROMs | :heavy_check_mark: using `I2C` or `SPI` driver |
| EEPROM emulation | Support planned (no ETA)                       |
| serial driver    | :heavy_check_mark: using `SIO` or `PIO` driver |
| UART driver      | Support planned (no ETA)                       |

## GPIO pin nomenclature

<img alt="Raspberry Pi Pico Pinout" src="gitbook/images/Pi_Pico_Pinout.webp" width="48%"/>
<img alt="Adafruit KB2040 Pinout" src="gitbook/images/KB2040_Pinout.webp" width="48%"/>

!> The GPIO pins of the RP2040 are not 5V tolerant!

To address individual pins on the RP2040, QMK uses the `GPx` abbreviation -- where the `x` stands for the GPIO number of the pin. This number can likely be found on the official pinout diagram of your board. Note that these GPIO numbers match the RP2040 MCU datasheet, and don't necessarily match the number you see printed on the board. For instance the Raspberry Pi Pico uses numbers from 1 to 40 for their pins, but these are not identical to the RP2040's GPIO numbers. So if you want to use the pin 11 of the Pico for your keyboard, you would refer to it as `GP8` in the config files.

## Double-tap reset boot-loader entry

The double-tap reset mechanism is an alternate way in QMK to enter the embedded mass storage UF2 boot-loader of the RP2040. It works by a fast double-tap of the reset pin on start up, which is similar to the behavior of AVR Pro Micros. This feature is not activated by default and has to be configured. To activate it add the following options to your keyboards `config.h` file:

```c
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET // Activates the double-tap behavior
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 200U // Timeout window in ms in which the double tap can occur.
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_LED_MASK 0U // Specify a optional status led which blinks when entering the bootloader
```

## Pre-defined RP2040 boards

QMK defines two boards that you can choose from to base your RP2040 powered keyboard upon. These boards provide pre-configured default pins and drivers.

### Generic Pro Micro RP2040

This is the default board that is chosen, unless any other RP2040 board is selected in your keyboards `rules.mk` file. It assumes a pin layout for the I2C, SPI and Serial drivers which is identical to the Sparkfun Pro Micro RP2040, all values can be overwritten by defining them in your keyboards `config.h` file.

| Driver configuration define                                                | Value                                |
| -------------------------------------------------------------------------- | ------------------------------------ |
| **I2C driver**                                                             |                                      |
| `I2C_DRIVER`                                                               | `I2CD2`                              |
| `I2C1_SDA_PIN`                                                             | `GP2`                                |
| `I2C1_SCL_PIN`                                                             | `GP3`                                |
| **SPI driver**                                                             |                                      |
| `SPI_DRIVER`                                                               | `SPID0`                              |
| `SPI_SCK_PIN`                                                              | `GP18`                               |
| `SPI_MISO_PIN`                                                             | `GP20`                               |
| `SPI_MOSI_PIN`                                                             | `GP19`                               |
| **Serial driver**                                                          |                                      |
| `SERIAL_USART_DRIVER` ([SIO Driver](serial_driver.md#the-sio-driver) only) | `SIOD0`                              |
| `SOFT_SERIAL_PIN`                                                          | undefined, use `SERIAL_USART_TX_PIN` |
| `SERIAL_USART_TX_PIN`                                                      | `GP0`                                |
| `SERIAL_USART_RX_PIN`                                                      | `GP1`                                |

### Generic RP2040 board

This board can be chosen as a base for RP2040 keyboards that configure all necessary pins and drivers themselves and do not want to build their configuration on top of the Generic Pro Micro RP2040 board. Thus it doesn't provide any pre-configured pins or drivers. To select this board add the following line to your keyboards `rules.mk` file.

```make
BOARD = GENERIC_RP_RP2040
```

## Split keyboard support

Split keyboards are fully supported using the [serial driver](serial_driver.md) in both Full-duplex and Half-duplex configurations. For this to work two driver subsystems are supported by the RP2040, the hardware UART based `SIO` and the Programmable IO based `PIO` driver.

| Feature                       | [SIO Driver](serial_driver.md#the-sio-driver) | [PIO Driver](serial_driver.md#the-pio-driver) |
| ----------------------------- | --------------------------------------------- | --------------------------------------------- |
| Half-Duplex operation         |                                               | :heavy_check_mark:                            |
| Full-Duplex operation         | :heavy_check_mark:                            | :heavy_check_mark:                            |
| `TX` and `RX` pin swapping    |                                               | :heavy_check_mark:                            |
| Any GPIO as `TX` and `RX` pin | Only UART capable pins                        | :heavy_check_mark:                            |
| Simple configuration          |                                               | :heavy_check_mark:                            |

The `PIO` driver is much more flexible then the `SIO` driver, the only "downside" is the usage of `PIO` resources which in turn are not available for advanced user programs. Under normal circumstances, this resource allocation will be a non-issue.

## RP2040 second stage bootloader selection

As the RP2040 does not have any internal flash memory it depends on an external SPI flash memory chip to store and execute instructions from. To successfully interact with a wide variety of these chips a second stage bootloader that is compatible with the chosen external flash memory has to be supplied with each firmware image. By default an `W25Q080` compatible bootloader is assumed, but others can be chosen by adding one of the defines listed in the table below to your keyboards `config.h` file. 

| Compatible with flash chip | Selection                          |
| :------------------------- | ---------------------------------- |
| W25Q080                    | Selected by default                |
| AT25SF128A                 | `#define RP2040_FLASH_AT25SF128A`  |
| GD25Q64CS                  | `#define RP2040_FLASH_GD25Q64CS`   |
| W25X10CL                   | `#define RP2040_FLASH_W25X10CL`    |
| IS25LP080                  | `#define RP2040_FLASH_IS25LP080`   |
| Generic 03H flash          | `#define RP2040_FLASH_GENERIC_03H` |
