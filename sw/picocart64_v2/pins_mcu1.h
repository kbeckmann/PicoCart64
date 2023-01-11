/**
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Konrad Beckmann
 */

#pragma once

#define PIN_N64_AD0         ( 0)
#define PIN_N64_AD1         ( 1)
#define PIN_N64_AD2         ( 2)
#define PIN_N64_AD3         ( 3)
#define PIN_N64_AD4         ( 4)
#define PIN_N64_AD5         ( 5)
#define PIN_N64_AD6         ( 6)
#define PIN_N64_AD7         ( 7)
#define PIN_N64_AD8         ( 8)
#define PIN_N64_AD9         ( 9)
#define PIN_N64_AD10        (10)
#define PIN_N64_AD11        (11)
#define PIN_N64_AD12        (12)
#define PIN_N64_AD13        (13)
#define PIN_N64_AD14        (14)
#define PIN_N64_AD15        (15)
#define PIN_N64_ALEL        (16)
#define PIN_N64_ALEH        (17)
#define PIN_N64_WRITE       (18)
#define PIN_N64_READ        (19)
#define PIN_N64_COLD_RESET  (20)
#define PIN_N64_SI_DAT      (21)
#define PIN_N64_INT1        (22)
#define PIN_DEMUX_A0        (23)
#define PIN_DEMUX_A1        (24)
#define PIN_DEMUX_A2        (25)
#define PIN_DEMUX_IE        (26)
#define PIN_MCU2_SCK        (27) // MCU2 GPIO pin 26: PIN_SPI1_SCK
#define PIN_MCU2_CS         (28) // MCU2 GPIO pin 29: PIN_SPI1_CS
#define PIN_MCU2_DIO        (29) // MCU2 GPIO pin 28: PIN_SPI1_RX

#define PIO_UART_BAUD_RATE 115200

#define DEBUG_UART uart0
#define DEBUG_UART_BAUD_RATE 115200
#define DEBUG_UART_TX_PIN (PIN_MCU2_CS)
#define DEBUG_UART_RX_PIN (-1)
