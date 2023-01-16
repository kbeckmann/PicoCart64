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

volatile uint8_t processedCommands[128];
volatile uint32_t processedCommandsIndex = 0;
volatile uint8_t processedCommandsLooped = 0;
volatile uint8_t eeprom[16 * 1024 / 8];
uint16_t eeprom_type = EEPROM_TYPE_16K;


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

// uint joybus_offset_outmode = 0;

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

void __time_critical_func(enterMode)(int dataPin) {
    gpio_init(dataPin);
    gpio_set_dir(dataPin, GPIO_IN);
    gpio_pull_up(dataPin);

    sleep_us(100); // Stabilize voltages

    PIO pio = pio1;
    pio_gpio_init(pio, dataPin);
    uint offset = pio_add_program(pio, &joybus_program);

    pio_sm_config config = joybus_program_get_default_config(offset);
    sm_config_set_in_pins(&config, dataPin);
    sm_config_set_out_pins(&config, dataPin, 1);
    sm_config_set_set_pins(&config, dataPin, 1);
    // float div = ((float) (clock_get_hz(clk_sys))) / (32 * 250000);
    sm_config_set_clkdiv(&config, 8);
    sm_config_set_out_shift(&config, true, false, 32);
    sm_config_set_in_shift(&config, false, true, 8);
    
    pio_sm_init(pio, 0, offset, &config);
    pio_sm_set_enabled(pio, 0, true);
    
    while (true) {
        uint8_t buffer[3];
        buffer[0] = pio_sm_get_blocking(pio, 0); 

        // processedCommands[processedCommandsIndex++] = buffer[0];
        processedCommandsIndex++;

        if (buffer[0] == 0) { // Probe
            // uint8_t probeResponse[3] = { 0x00, 0x80, 0x00 };
            uint8_t probeResponse[3] = { 0x00, 0xC0, 0x00 };
            uint32_t result[2];
            int resultLen;
            convertToPio(probeResponse, 3, result, &resultLen);
            sleep_us(5); // 3.75us into the bit before end bit => 6.25 to wait if the end-bit is 5us long

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
            // if (blockToRead != 0) {
            //     printf("%u ", blockToRead);
            // }
        }
        else if (buffer[0] == JOYBUS_CMD_EEPROM_WRITE) {
            buffer[0] = pio_sm_get_blocking(pio, 0); // read the block number to write
            uint8_t blockToWrite = buffer[0];

            uint32_t eepromBlockStartingIndex = blockToWrite * 8;
            for (int i = 0; i < 8; i++) {
                buffer[0] = pio_sm_get_blocking(pio, 0);
                eeprom[eepromBlockStartingIndex + i] = (uint8_t)(buffer[0] & 0xFF);
            }
            
            uint8_t probeResponse[1] = { 0x00 };
            uint32_t result[2];
            int resultLen;
            convertToPio(probeResponse, 1, result, &resultLen);
            
            pio_sm_set_enabled(pio, 0, false);
            pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
            pio_sm_set_enabled(pio, 0, true);

            for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
            // if (blockToWrite > 0) {
            //     printf("\nWrote to eeprom[%u].\n", blockToWrite);
            // }
        }
        // else if (buffer[0] == 0x41) { // Origin (NOT 0x81)
        //     gpio_put(25, 1);
        //     uint8_t originResponse[10] = { 0x00, 0x80, 128, 128, 128, 128, 0, 0, 0, 0 };
        //     // TODO The origin response sends centered values in this code excerpt. Consider whether that makes sense for your project (digital controllers -> yes)
        //     uint32_t result[6];
        //     int resultLen;
        //     convertToPio(originResponse, 10, result, resultLen);
        //     // Here we don't wait because convertToPio takes time

        //     pio_sm_set_enabled(pio, 0, false);
        //     pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
        //     pio_sm_set_enabled(pio, 0, true);

        //     for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
        // }
        // else if (buffer[0] == 0x40) { // Could check values past the first byte for reliability
        //     buffer[0] = pio_sm_get_blocking(pio, 0);
        //     buffer[0] = pio_sm_get_blocking(pio, 0);
        //     gpio_put(rumblePin, buffer[0] & 1);

        //     //TODO The call to the state building function happens here, because on digital controllers, it's near instant, so it can be done between the poll and the response
        //     // It must be very fast (few us max) to be done between poll and response and still be compatible with adapters
        //     // Consider whether that makes sense for your project. If your state building is long, use a different control flow i.e precompute somehow and have func read it
        //     //Also consider that you may need to mark this function as a __time_critical_func to ensure that it's run from RAM and not flash.
        //     GCReport gcReport = func();

        //     uint32_t result[5];
        //     int resultLen;
        //     convertToPio((uint8_t*)(&gcReport), 8, result, resultLen);

        //     pio_sm_set_enabled(pio, 0, false);
        //     pio_sm_init(pio, 0, offset+joybus_offset_outmode, &config);
        //     pio_sm_set_enabled(pio, 0, true);

        //     for (int i = 0; i<resultLen; i++) pio_sm_put_blocking(pio, 0, result[i]);
        // }
        else {
            pio_sm_set_enabled(pio, 0, false);
            sleep_us(400);
            //If an unmatched communication happens, we wait for 400us for it to finish for sure before starting to listen again
            pio_sm_init(pio, 0, offset+joybus_offset_inmode, &config);
            pio_sm_set_enabled(pio, 0, true);
            printf("Other cmd: %02x\n", buffer[0]);
        }

        if (processedCommandsIndex == 869) {
            // printf("%u ", processedCommandsIndex);
            for(int i = 0; i < 2048; i++) {
                if (i % 64 == 0) {
                    printf("\n");
                }
                printf("%02x ", eeprom[i]);
            }
        }
        if (processedCommandsIndex > 869) {
            printf("%u ", processedCommandsIndex);
        }
    }
}






// #include <string.h>
// #include <stdlib.h>
// #include <stdio.h>

// #include <hardware/clocks.h>
// #include <hardware/dma.h>
// #include <hardware/gpio.h>

// #include "joybus.h"
// #include "joybus.pio.h"

// typedef uint8_t (*joybus_callback_t) (uint8_t ch, uint8_t cmd, uint8_t rx_length, uint8_t *rx_buffer, uint8_t *tx_buffer);
// void joybus_init (PIO pio, uint8_t channels, uint *pins, joybus_callback_t callback);
// bool joybus_check_address_crc (uint16_t address);
// uint8_t joybus_calculate_data_crc (uint8_t *buffer);

// typedef struct {
//     uint sm;
//     struct {
//         uint id;
//         dma_channel_config rx, tx;
//     } dma;
//     struct {
//         uint8_t length;
//         uint8_t data[JOYBUS_BUFFER_SIZE];
//     } rx, tx;
// } joybus_channel_t;

// typedef struct {
//     PIO pio;
//     struct {
//         uint pio, dma;
//     } irq;
//     uint offset;
//     joybus_callback_t callback;
//     uint8_t channels;
//     joybus_channel_t *channel;
// } joybus_t;


// static joybus_t *joybus = NULL;

// static const uint8_t __not_in_flash("joybus") joybus_address_crc_table[11] = {
//     0x01, 0x1A, 0x0D, 0x1C, 0x0E, 0x07, 0x19, 0x16, 0x0B, 0x1F, 0x15
// };

// static const uint8_t __not_in_flash("joybus") joybus_data_crc_table[256] = {
//     0x00, 0x85, 0x8F, 0x0A, 0x9B, 0x1E, 0x14, 0x91, 0xB3, 0x36, 0x3C, 0xB9, 0x28, 0xAD, 0xA7, 0x22,
//     0xE3, 0x66, 0x6C, 0xE9, 0x78, 0xFD, 0xF7, 0x72, 0x50, 0xD5, 0xDF, 0x5A, 0xCB, 0x4E, 0x44, 0xC1,
//     0x43, 0xC6, 0xCC, 0x49, 0xD8, 0x5D, 0x57, 0xD2, 0xF0, 0x75, 0x7F, 0xFA, 0x6B, 0xEE, 0xE4, 0x61,
//     0xA0, 0x25, 0x2F, 0xAA, 0x3B, 0xBE, 0xB4, 0x31, 0x13, 0x96, 0x9C, 0x19, 0x88, 0x0D, 0x07, 0x82,
//     0x86, 0x03, 0x09, 0x8C, 0x1D, 0x98, 0x92, 0x17, 0x35, 0xB0, 0xBA, 0x3F, 0xAE, 0x2B, 0x21, 0xA4,
//     0x65, 0xE0, 0xEA, 0x6F, 0xFE, 0x7B, 0x71, 0xF4, 0xD6, 0x53, 0x59, 0xDC, 0x4D, 0xC8, 0xC2, 0x47,
//     0xC5, 0x40, 0x4A, 0xCF, 0x5E, 0xDB, 0xD1, 0x54, 0x76, 0xF3, 0xF9, 0x7C, 0xED, 0x68, 0x62, 0xE7,
//     0x26, 0xA3, 0xA9, 0x2C, 0xBD, 0x38, 0x32, 0xB7, 0x95, 0x10, 0x1A, 0x9F, 0x0E, 0x8B, 0x81, 0x04,
//     0x89, 0x0C, 0x06, 0x83, 0x12, 0x97, 0x9D, 0x18, 0x3A, 0xBF, 0xB5, 0x30, 0xA1, 0x24, 0x2E, 0xAB,
//     0x6A, 0xEF, 0xE5, 0x60, 0xF1, 0x74, 0x7E, 0xFB, 0xD9, 0x5C, 0x56, 0xD3, 0x42, 0xC7, 0xCD, 0x48,
//     0xCA, 0x4F, 0x45, 0xC0, 0x51, 0xD4, 0xDE, 0x5B, 0x79, 0xFC, 0xF6, 0x73, 0xE2, 0x67, 0x6D, 0xE8,
//     0x29, 0xAC, 0xA6, 0x23, 0xB2, 0x37, 0x3D, 0xB8, 0x9A, 0x1F, 0x15, 0x90, 0x01, 0x84, 0x8E, 0x0B,
//     0x0F, 0x8A, 0x80, 0x05, 0x94, 0x11, 0x1B, 0x9E, 0xBC, 0x39, 0x33, 0xB6, 0x27, 0xA2, 0xA8, 0x2D,
//     0xEC, 0x69, 0x63, 0xE6, 0x77, 0xF2, 0xF8, 0x7D, 0x5F, 0xDA, 0xD0, 0x55, 0xC4, 0x41, 0x4B, 0xCE,
//     0x4C, 0xC9, 0xC3, 0x46, 0xD7, 0x52, 0x58, 0xDD, 0xFF, 0x7A, 0x70, 0xF5, 0x64, 0xE1, 0xEB, 0x6E,
//     0xAF, 0x2A, 0x20, 0xA5, 0x34, 0xB1, 0xBB, 0x3E, 0x1C, 0x99, 0x93, 0x16, 0x87, 0x02, 0x08, 0x8D,
// };


// static inline void __time_critical_func(joybus_process_packet) (uint8_t ch) {
//     joybus_channel_t *channel = &joybus->channel[ch];

//     channel->tx.length = 0;

//     if (channel->rx.length == 0) {
//         return;
//     }

//     memset(channel->tx.data, 0, JOYBUS_BUFFER_SIZE);

//     channel->tx.length = joybus->callback(
//         ch,
//         channel->rx.data[0],
//         channel->rx.length - 1,
//         &channel->rx.data[1],
//         channel->tx.data
//     );
// }

// static void __time_critical_func(joybus_finish_receive_data) (uint ch) {
//     joybus_channel_t *channel = &joybus->channel[ch];
//     dma_channel_hw_t *dma = dma_channel_hw_addr(channel->dma.id);
//     channel->rx.length = (JOYBUS_BUFFER_SIZE - dma->transfer_count);
//     dma_channel_abort(channel->dma.id);
// }

// static void __time_critical_func(joybus_start_receive_data) (uint ch) {
//     joybus_channel_t *channel = &joybus->channel[ch];
//     dma_channel_set_config(channel->dma.id, &channel->dma.rx, false);
//     dma_channel_set_read_addr(channel->dma.id, &joybus->pio->rxf[channel->sm], false);
//     dma_channel_set_irq0_enabled(channel->dma.id, false);
//     dma_channel_transfer_to_buffer_now(channel->dma.id, channel->rx.data, JOYBUS_BUFFER_SIZE);
// }

// static void __time_critical_func(joybus_start_transmit_data) (uint ch) {
//     joybus_channel_t *channel = &joybus->channel[ch];
//     uint32_t transfer_count = (sizeof(channel->tx.length) + channel->tx.length);
//     dma_channel_set_config(channel->dma.id, &channel->dma.tx, false);
//     dma_channel_set_write_addr(channel->dma.id, &joybus->pio->txf[channel->sm], false);
//     dma_channel_set_irq0_enabled(channel->dma.id, true);
//     dma_channel_transfer_from_buffer_now(channel->dma.id, &channel->tx, transfer_count);
//     uint jmp_to_tx_start = pio_encode_jmp(joybus->offset + joybus_offset_tx_start);
//     pio_sm_exec_wait_blocking(joybus->pio, channel->sm, jmp_to_tx_start);
// }

// static void __time_critical_func(joybus_pio_irq_handler) (void) {
//     for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
//         if (pio_interrupt_get(joybus->pio, joybus->channel[ch].sm)) {
//             pio_interrupt_clear(joybus->pio, joybus->channel[ch].sm);
//             joybus_finish_receive_data(ch);
//             joybus_process_packet(ch);
//             if (joybus->channel[ch].tx.length == 0) {
//                 joybus_start_receive_data(ch);
//             } else {
//                 joybus_start_transmit_data(ch);
//             }
//         }
//     }
// }

// static void __time_critical_func(joybus_dma_irq_handler) (void) {
//     for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
//         if (dma_channel_get_irq0_status(joybus->channel[ch].dma.id)) {
//             dma_channel_acknowledge_irq0(joybus->channel[ch].dma.id);
//             joybus_start_receive_data(ch);
//         }
//     }
// }

// static void joybus_program_init (PIO pio, uint sm, uint offset, uint pin) {
//     pio_sm_config c = joybus_program_get_default_config(offset);

//     sm_config_set_out_pins(&c, pin, 1);
//     sm_config_set_set_pins(&c, pin, 1);
//     sm_config_set_in_pins(&c, pin);
//     sm_config_set_jmp_pin(&c, pin);

//     sm_config_set_out_shift(&c, false, true, 8);
//     sm_config_set_in_shift(&c, false, false, 8);

//     float div = ((float) (clock_get_hz(clk_sys))) / (32 * 250000);
//     sm_config_set_clkdiv(&c, div);

//     gpio_pull_up(pin);
//     pio_sm_set_pins_with_mask(pio, sm, (1 << pin), (1 << pin));
//     pio_sm_set_pindirs_with_mask(pio, sm, (1 << pin), (1 << pin));
//     pio_gpio_init(pio, pin);
//     gpio_set_oeover(pin, GPIO_OVERRIDE_INVERT);
//     pio_sm_set_pins_with_mask(pio, sm, 0, (1 << pin));

//     pio_interrupt_clear(pio, sm);
//     pio_set_irq0_source_enabled(pio, pis_interrupt0 + sm, true);

//     pio_sm_init(pio, sm, offset + joybus_offset_rx_start, &c);
//     pio_sm_set_enabled(pio, sm, true);
// }

// static void joybus_dma_init (PIO pio, joybus_channel_t *channel) {
//     channel->dma.id = dma_claim_unused_channel(true);

//     channel->dma.rx = dma_channel_get_default_config(channel->dma.id);
//     channel_config_set_transfer_data_size(&channel->dma.rx, DMA_SIZE_8);
//     channel_config_set_dreq(&channel->dma.rx, pio_get_dreq(pio, channel->sm, false));
//     channel_config_set_read_increment(&channel->dma.rx, false);
//     channel_config_set_write_increment(&channel->dma.rx, true);

//     channel->dma.tx = dma_channel_get_default_config(channel->dma.id);
//     channel_config_set_transfer_data_size(&channel->dma.tx, DMA_SIZE_8);
//     channel_config_set_dreq(&channel->dma.tx, pio_get_dreq(pio, channel->sm, true));
//     channel_config_set_read_increment(&channel->dma.tx, true);
//     channel_config_set_write_increment(&channel->dma.tx, false);
// }


// void joybus_init (PIO pio, uint8_t channels, uint *pins, joybus_callback_t callback) {
//     assert(!joybus);
//     assert(channels > 0);
//     assert(channels <= 4);
//     assert(pins);
//     assert(callback);

//     joybus = malloc(sizeof(joybus_t));
//     assert(joybus);
//     joybus->pio = pio;
//     joybus->offset = pio_add_program(joybus->pio, &joybus_program);
//     joybus->callback = callback;
//     joybus->channels = channels;
//     joybus->channel = malloc(sizeof(joybus_channel_t) * joybus->channels);
//     assert(joybus->channel);

//     for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
//         joybus->channel[ch].sm = pio_claim_unused_sm(joybus->pio, true);
//         joybus_dma_init(joybus->pio, &joybus->channel[ch]);
//         joybus_program_init(joybus->pio, joybus->channel[ch].sm, joybus->offset, pins[ch]);
//         joybus_start_receive_data(ch);
//     }

//     joybus->irq.dma = DMA_IRQ_0;
//     irq_add_shared_handler(joybus->irq.dma, joybus_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
//     irq_set_priority(joybus->irq.dma, PICO_HIGHEST_IRQ_PRIORITY);
//     irq_set_enabled(joybus->irq.dma, true);

//     joybus->irq.pio = pio_get_index(joybus->pio) == 0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
//     irq_add_shared_handler(joybus->irq.pio, joybus_pio_irq_handler, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
//     irq_set_priority(joybus->irq.pio, PICO_HIGHEST_IRQ_PRIORITY);
//     irq_set_enabled(joybus->irq.pio, true);
// }

// bool __time_critical_func(joybus_check_address_crc) (uint16_t address) {
//     uint8_t crc = 0x00;
//     for (int i = 0; i < 11; i += 1) {
//         if (address & (1 << (15 - i))) {
//             crc ^= joybus_address_crc_table[i];
//         }
//     }
//     return (crc == (address & 0x1F));
// }

// uint8_t __time_critical_func(joybus_calculate_data_crc) (uint8_t *buffer) {
//     uint8_t crc = 0x00;
//     for (uint8_t byte = 0; byte < 32; byte += 1) {
//         crc = joybus_data_crc_table[crc ^ buffer[byte]];
//     }
//     return crc;
// }

// uint8_t __time_critical_func(joybus_callback) (uint8_t ch, uint8_t cmd, uint8_t rx_length, uint8_t *rx_buffer, uint8_t *tx_buffer) {
//     uint8_t tx_length = 0;

//     if (ch > 0) {
//         processedCommands[processedCommandsIndex++] = 0xFA;
//         return 0;
//     }

//     switch (cmd) {
//         case JOYBUS_CMD_INFO:
//             processedCommands[processedCommandsIndex++] = JOYBUS_CMD_INFO;
//             if (rx_length == 0) {
//                 uint32_t info = 0x008000;
//                 tx_length = 3;
//                 tx_buffer[0] = ((info >> 16) & 0xFF);
//                 tx_buffer[1] = ((info >> 8) & 0xFF);
//                 tx_buffer[2] = (info & 0xFF);
//             }
//             break;

//         case JOYBUS_CMD_EEPROM_READ:
//             processedCommands[processedCommandsIndex++] = JOYBUS_CMD_EEPROM_READ;
//             if (rx_length != 0) {
//                 uint8_t blockToRead = rx_buffer[0] * 8;
//                 tx_length = 8;
//                 for (int i = 0; i < 8; i++) {
//                     tx_buffer[i] = eeprom[blockToRead+i];
//                 }
//             }
//             break;

//         case JOYBUS_CMD_EEPROM_WRITE:
//             processedCommands[processedCommandsIndex++] = JOYBUS_CMD_EEPROM_WRITE;
//             if (rx_length != 0) {
//                 tx_length = 1;
//                 tx_buffer[0] = 0x00;

//                 uint8_t blockToWrite = rx_buffer[0] * 8;
//                 // 8 bytes of data
//                 for(int i = 0; i < 8; i++) {
//                     eeprom[blockToWrite+i] = rx_buffer[i];
//                 }
//             }
//             break;

//         case JOYBUS_CMD_RESET:
//             break;
//     }

//     if (processedCommandsIndex >= 128) {
//         processedCommandsIndex = 0;
//         processedCommandsLooped++;
//     }

//     return tx_length;
// }

void enable_joybus() {
    printf("Enabling joybus\n");
    // uint joybus_pins[1] = { 21 };
    // joybus_init(pio1, 1, joybus_pins, joybus_callback);

    for(int i = 0; i < 2048; i++) {
        eeprom[i] = i / 8;
    }

    enterMode(21);
}

void disable_joybus() {
    // pio_sm_set_enabled(joybus->pio, joybus->channel[0].sm, false);
    // pio_remove_program(joybus->pio, &joybus_program, joybus->offset);
}

void change_eeprom_type(uint16_t newType) {
    if (newType != EEPROM_TYPE_4K || newType != EEPROM_TYPE_16K) {
        eeprom_type = EEPROM_TYPE_4K; // default to 4kb
    } else {
        eeprom_type = newType;
    }
}

void dump_joybus_debug_info() {
    printf("Processed %u commands, looped %u times.\n", processedCommandsIndex, processedCommandsLooped);
    for (int i = 0; i < processedCommandsIndex; i++) {
        printf("%02x ", processedCommands[processedCommandsIndex]);
    }

    processedCommandsIndex = 0;
    processedCommandsLooped = 0;
}