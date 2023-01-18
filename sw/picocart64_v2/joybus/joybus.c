#include "joybus.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "pico/time.h"
#include "pico/platform.h"
#include "hardware/gpio.h"
#include <hardware/clocks.h>
#include "hardware/pio.h"
#include "joybus.pio.h"

#include "pio_uart/pio_uart.h"

volatile uint16_t eeprom_type = EEPROM_TYPE_4K; // default to 4K eeprom
volatile uint8_t eeprom[2048]; // sized to fit the 16K eeprom

#define COMMAND_START    0xDE
#define COMMAND_START2   0xAD
#define COMMAND_BACKUP_EEPROM  (0xBE)
// Send eeprom data to mcu2
void sendEepromData() {
    uint8_t backupCommand; 
    uint16_t numBytesToSend;

    if (eeprom_type == EEPROM_TYPE_4K) {
        numBytesToSend = 512;
    } else {
        numBytesToSend = 2048;
    }

    uart_tx_program_putc(COMMAND_START);
    uart_tx_program_putc(COMMAND_START2);
    uart_tx_program_putc(COMMAND_BACKUP_EEPROM);
    uart_tx_program_putc((uint8_t)(numBytesToSend >> 8));
    uart_tx_program_putc((uint8_t)(numBytesToSend));
    
    for (int i = 0; i < numBytesToSend; i++) {
        while (!uart_tx_program_is_writable()) {
            tight_loop_contents();
        }
        uart_tx_program_putc(eeprom[i]);
    }
}

/* PIOs are separate state machines for handling IOs with high timing precision. You load a program into them and they do their stuff on their own with deterministic timing,
   communicating with the main cores via FIFOs (and interrupts, if you want).

   Attempting to bit-bang the Joybus protocol by writing down timer snapshots upon GPIO changes and waiting on timers to trigger GPIO changes is bound to encounter some
   issues related to the uncertainty of timings (the STM32F0 is pipelined). Previous versions of this used to do that and would occasionally have the controller disconnect
   for a frame due to a polling fault, which would manifest particularly on the CSS by the hand teleporting back to its default position.
   Ever since the migration to PIOs - the proper tool for the job - this problem is gone.

   This project is adapted from the communication module of pico-rectangle (https://github.com/JulienBernard3383279/pico-rectangle), a digital controller firmware.

   The PIO program expects the system clock to be 125MHz, but you can adapt it by fiddling with the delays in the PIO program.

   One PIO here is configured to wait for new bytes over the joybus protocol. The main core spins a byte to come from the PIO FIFO, which will be the Joybus command
   from the console. It then matches that byte to know whether this is probe, origin or poll request.
   Once it has matched a command and has let the console finish talking, it will start replying, but in the case of the poll command, it will first build the state.
   The state is built by calling a callback passed as parameter to enterMode. This is fine for digital controllers because it takes few microseconds, but isn't
   if your state building is going to take any longer. (The console is fine with some delay between the poll command and response, but adapters don't tolerate more than
   few microseconds) In that case, you'll need to change the control flow for the passed callback to not do any computational work itself.

   Check the T O D O s in this file for things you should check when adapting it to your project.

   I advise checking the RP2040 documentation and this video https://www.youtube.com/watch?v=yYnQYF_Xa8g&ab_channel=stacksmashing to understand
*/

PIO pio = pio1;
uint joybus_pio_offset;

void __time_critical_func(convertToPio)(const uint8_t* command, const int len, uint32_t* result, int* resultLen) {
    // PIO Shifts to the right by default
    // In: pushes batches of 8 shifted left, i.e we get [0x40, 0x03, rumble (the end bit is never pushed)]
    // Out: We push commands for a right shift with an enable pin, ie 5 (101) would be 0b11'10'11
    // So in doesn't need post processing but out does
    if (len == 0) {
        *resultLen = 0;
        return;
    }
    *resultLen = len/2 + 1;
    int i;
    for (i = 0; i < *resultLen; i++) {
        result[i] = 0;
    }
    for (i = 0; i < len; i++) {
        for (int j = 0; j < 8; j++) {
            result[i / 2] += 1 << (2 * (8 * (i % 2) + j) + 1);
            result[i / 2] += (!!(command[i] & (0x80u >> j))) << (2 * (8 * (i % 2) + j));
        }
    }
    // End bit
    result[len / 2] += 3 << (2 * (8 * (len % 2)));
}

void __time_critical_func(processJoybus)(int dataPin) {
    gpio_init(dataPin);
    gpio_set_dir(dataPin, GPIO_IN);
    gpio_pull_up(dataPin);

    sleep_us(100); // Stabilize voltages

    pio_gpio_init(pio, dataPin);
    uint offset = pio_add_program(pio, &joybus_program);
    joybus_pio_offset = offset;

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, dataPin);
    sm_config_set_out_pins(&config, dataPin, 1);
    sm_config_set_set_pins(&config, dataPin, 1);

    sm_config_set_clkdiv(&config, 10.6);

    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);
    
    pio_sm_init(pio, 0, offset, &config);
    pio_sm_set_enabled(pio, 0, true);
    
    bool resetStateChanged = false;
    bool lastResetState = false;
    uint32_t waitTime = (1 * 60 * 1000000); 
    uint32_t startTime = time_us_32();
    volatile uint8_t buffer[3] = {0};
    while (true) {
        if(pio_sm_is_rx_fifo_empty(pio, 0)) {
            uint32_t now = time_us_32();
            uint32_t diff = now - startTime;
            if (diff > waitTime) {
                printf("Dumping eeprom. Start-time: %u, now: %u, diff: %u\n", startTime, now, diff);
                startTime = now;

                // Dump eeprom
                // for(int i = 0; i < 512; i++) {
                //     if (i % 8 == 0) {
                //         printf("\n%04x: ", i);
                //     }
                //     printf("%02x ", eeprom[i]);
                // }
                // Save eeprom to sd card
                sendEepromData();
            }
            continue; // don't process loop
        } else {
            buffer[0] = pio_sm_get(pio, 0); 
        }

        if (buffer[0] == 0) { // INFO
            uint8_t probeResponse[3] = { 0x00, 0x80, 0x00 };
            // uint8_t probeResponse[3] = { 0x00, 0xC0, 0x00 };
            uint32_t result[2];
            int resultLen;
            convertToPio(probeResponse, 3, result, &resultLen);
            sleep_us(6); // 3.75us into the bit before end bit => 6.25 to wait if the end-bit is 5us long

            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
        }
        else if (buffer[0] == JOYBUS_CMD_EEPROM_READ) {
            buffer[0] = pio_sm_get_blocking(pio, 0); // read the block number to read
            uint8_t blockToRead = buffer[0];
            uint32_t eepromOffset = blockToRead * 8;
            uint8_t dataToSend[8];
            for(int i = 0; i < 8; i++) {
                dataToSend[i] = eeprom[eepromOffset + i];
            }

            uint32_t result[9];
            int resultLen;
            convertToPio(dataToSend, 8, result, &resultLen);

            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
        }
        else if (buffer[0] == JOYBUS_CMD_EEPROM_WRITE) {
            buffer[0] = pio_sm_get_blocking(pio, 0); // read the block number to write
            uint8_t blockToWrite = buffer[0];

            uint32_t eepromBlockStartingIndex = blockToWrite * 8;
            // printf("%04x: ", eepromBlockStartingIndex);
            for (int i = 0; i < 8; i++) {
                buffer[0] = pio_sm_get_blocking(pio, 0);
                eeprom[eepromBlockStartingIndex + i] = buffer[0];
                printf("%02x ", buffer[0]);
            }
            printf("\n");

            uint32_t result[2];
            int resultLen = 1;
            result[0] = 0x0003aaaa;
        
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);

            
        }
        else {
            pio_sm_set_enabled(pio, 0, false);
            sleep_us(400);
            //If an unmatched communication happens, we wait for 400us for it to finish for sure before starting to listen again
            pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
            pio_sm_set_enabled(pio, 0, true);
            printf("Other cmd: %02x\n", buffer[0]);
            uart_tx_program_putc(buffer[0]);
        }
    }
}

void enable_joybus() {
    printf("Enabling joybus\n");
    processJoybus(21); // on pin 21
}

void disable_joybus() {
    pio_sm_set_enabled(pio, 0, false);
    pio_remove_program(pio, &joybus_program, joybus_pio_offset);
}
