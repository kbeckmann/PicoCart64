#include <string.h>
#include <stdlib.h>

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>

#include "joybus.h"
#include "joybus.pio.h"

typedef uint8_t (*joybus_callback_t) (uint8_t ch, uint8_t cmd, uint8_t rx_length, uint8_t *rx_buffer, uint8_t *tx_buffer);
void joybus_init (PIO pio, uint8_t channels, uint *pins, joybus_callback_t callback);
bool joybus_check_address_crc (uint16_t address);
uint8_t joybus_calculate_data_crc (uint8_t *buffer);

uint8_t eeprom[2048];
uint16_t eeprom_type = EEPROM_TYPE_4K;


typedef struct {
    uint sm;
    struct {
        uint id;
        dma_channel_config rx, tx;
    } dma;
    struct {
        uint8_t length;
        uint8_t data[JOYBUS_BUFFER_SIZE];
    } rx, tx;
} joybus_channel_t;

typedef struct {
    PIO pio;
    struct {
        uint pio, dma;
    } irq;
    uint offset;
    joybus_callback_t callback;
    uint8_t channels;
    joybus_channel_t *channel;
} joybus_t;


static joybus_t *joybus = NULL;

static const uint8_t __not_in_flash("joybus") joybus_address_crc_table[11] = {
    0x01, 0x1A, 0x0D, 0x1C, 0x0E, 0x07, 0x19, 0x16, 0x0B, 0x1F, 0x15
};

static const uint8_t __not_in_flash("joybus") joybus_data_crc_table[256] = {
    0x00, 0x85, 0x8F, 0x0A, 0x9B, 0x1E, 0x14, 0x91, 0xB3, 0x36, 0x3C, 0xB9, 0x28, 0xAD, 0xA7, 0x22,
    0xE3, 0x66, 0x6C, 0xE9, 0x78, 0xFD, 0xF7, 0x72, 0x50, 0xD5, 0xDF, 0x5A, 0xCB, 0x4E, 0x44, 0xC1,
    0x43, 0xC6, 0xCC, 0x49, 0xD8, 0x5D, 0x57, 0xD2, 0xF0, 0x75, 0x7F, 0xFA, 0x6B, 0xEE, 0xE4, 0x61,
    0xA0, 0x25, 0x2F, 0xAA, 0x3B, 0xBE, 0xB4, 0x31, 0x13, 0x96, 0x9C, 0x19, 0x88, 0x0D, 0x07, 0x82,
    0x86, 0x03, 0x09, 0x8C, 0x1D, 0x98, 0x92, 0x17, 0x35, 0xB0, 0xBA, 0x3F, 0xAE, 0x2B, 0x21, 0xA4,
    0x65, 0xE0, 0xEA, 0x6F, 0xFE, 0x7B, 0x71, 0xF4, 0xD6, 0x53, 0x59, 0xDC, 0x4D, 0xC8, 0xC2, 0x47,
    0xC5, 0x40, 0x4A, 0xCF, 0x5E, 0xDB, 0xD1, 0x54, 0x76, 0xF3, 0xF9, 0x7C, 0xED, 0x68, 0x62, 0xE7,
    0x26, 0xA3, 0xA9, 0x2C, 0xBD, 0x38, 0x32, 0xB7, 0x95, 0x10, 0x1A, 0x9F, 0x0E, 0x8B, 0x81, 0x04,
    0x89, 0x0C, 0x06, 0x83, 0x12, 0x97, 0x9D, 0x18, 0x3A, 0xBF, 0xB5, 0x30, 0xA1, 0x24, 0x2E, 0xAB,
    0x6A, 0xEF, 0xE5, 0x60, 0xF1, 0x74, 0x7E, 0xFB, 0xD9, 0x5C, 0x56, 0xD3, 0x42, 0xC7, 0xCD, 0x48,
    0xCA, 0x4F, 0x45, 0xC0, 0x51, 0xD4, 0xDE, 0x5B, 0x79, 0xFC, 0xF6, 0x73, 0xE2, 0x67, 0x6D, 0xE8,
    0x29, 0xAC, 0xA6, 0x23, 0xB2, 0x37, 0x3D, 0xB8, 0x9A, 0x1F, 0x15, 0x90, 0x01, 0x84, 0x8E, 0x0B,
    0x0F, 0x8A, 0x80, 0x05, 0x94, 0x11, 0x1B, 0x9E, 0xBC, 0x39, 0x33, 0xB6, 0x27, 0xA2, 0xA8, 0x2D,
    0xEC, 0x69, 0x63, 0xE6, 0x77, 0xF2, 0xF8, 0x7D, 0x5F, 0xDA, 0xD0, 0x55, 0xC4, 0x41, 0x4B, 0xCE,
    0x4C, 0xC9, 0xC3, 0x46, 0xD7, 0x52, 0x58, 0xDD, 0xFF, 0x7A, 0x70, 0xF5, 0x64, 0xE1, 0xEB, 0x6E,
    0xAF, 0x2A, 0x20, 0xA5, 0x34, 0xB1, 0xBB, 0x3E, 0x1C, 0x99, 0x93, 0x16, 0x87, 0x02, 0x08, 0x8D,
};


static inline void __time_critical_func(joybus_process_packet) (uint8_t ch) {
    joybus_channel_t *channel = &joybus->channel[ch];

    channel->tx.length = 0;

    if (channel->rx.length == 0) {
        return;
    }

    memset(channel->tx.data, 0, JOYBUS_BUFFER_SIZE);

    channel->tx.length = joybus->callback(
        ch,
        channel->rx.data[0],
        channel->rx.length - 1,
        &channel->rx.data[1],
        channel->tx.data
    );
}

static void __time_critical_func(joybus_finish_receive_data) (uint ch) {
    joybus_channel_t *channel = &joybus->channel[ch];
    dma_channel_hw_t *dma = dma_channel_hw_addr(channel->dma.id);
    channel->rx.length = (JOYBUS_BUFFER_SIZE - dma->transfer_count);
    dma_channel_abort(channel->dma.id);
}

static void __time_critical_func(joybus_start_receive_data) (uint ch) {
    joybus_channel_t *channel = &joybus->channel[ch];
    dma_channel_set_config(channel->dma.id, &channel->dma.rx, false);
    dma_channel_set_read_addr(channel->dma.id, &joybus->pio->rxf[channel->sm], false);
    dma_channel_set_irq0_enabled(channel->dma.id, false);
    dma_channel_transfer_to_buffer_now(channel->dma.id, channel->rx.data, JOYBUS_BUFFER_SIZE);
}

static void __time_critical_func(joybus_start_transmit_data) (uint ch) {
    joybus_channel_t *channel = &joybus->channel[ch];
    uint32_t transfer_count = (sizeof(channel->tx.length) + channel->tx.length);
    dma_channel_set_config(channel->dma.id, &channel->dma.tx, false);
    dma_channel_set_write_addr(channel->dma.id, &joybus->pio->txf[channel->sm], false);
    dma_channel_set_irq0_enabled(channel->dma.id, true);
    dma_channel_transfer_from_buffer_now(channel->dma.id, &channel->tx, transfer_count);
    uint jmp_to_tx_start = pio_encode_jmp(joybus->offset + joybus_offset_tx_start);
    pio_sm_exec_wait_blocking(joybus->pio, channel->sm, jmp_to_tx_start);
}

static void __time_critical_func(joybus_pio_irq_handler) (void) {
    for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
        if (pio_interrupt_get(joybus->pio, joybus->channel[ch].sm)) {
            pio_interrupt_clear(joybus->pio, joybus->channel[ch].sm);
            joybus_finish_receive_data(ch);
            joybus_process_packet(ch);
            if (joybus->channel[ch].tx.length == 0) {
                joybus_start_receive_data(ch);
            } else {
                joybus_start_transmit_data(ch);
            }
        }
    }
}

static void __time_critical_func(joybus_dma_irq_handler) (void) {
    for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
        if (dma_channel_get_irq0_status(joybus->channel[ch].dma.id)) {
            dma_channel_acknowledge_irq0(joybus->channel[ch].dma.id);
            joybus_start_receive_data(ch);
        }
    }
}

static void joybus_program_init (PIO pio, uint sm, uint offset, uint pin) {
    pio_sm_config c = joybus_program_get_default_config(offset);

    sm_config_set_out_pins(&c, pin, 1);
    sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_jmp_pin(&c, pin);

    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_in_shift(&c, false, false, 8);

    float div = ((float) (clock_get_hz(clk_sys))) / (32 * 250000);
    sm_config_set_clkdiv(&c, div);

    gpio_pull_up(pin);
    pio_sm_set_pins_with_mask(pio, sm, (1 << pin), (1 << pin));
    pio_sm_set_pindirs_with_mask(pio, sm, (1 << pin), (1 << pin));
    pio_gpio_init(pio, pin);
    gpio_set_oeover(pin, GPIO_OVERRIDE_INVERT);
    pio_sm_set_pins_with_mask(pio, sm, 0, (1 << pin));

    pio_interrupt_clear(pio, sm);
    pio_set_irq0_source_enabled(pio, pis_interrupt0 + sm, true);

    pio_sm_init(pio, sm, offset + joybus_offset_rx_start, &c);
    pio_sm_set_enabled(pio, sm, true);
}

static void joybus_dma_init (PIO pio, joybus_channel_t *channel) {
    channel->dma.id = dma_claim_unused_channel(true);

    channel->dma.rx = dma_channel_get_default_config(channel->dma.id);
    channel_config_set_transfer_data_size(&channel->dma.rx, DMA_SIZE_8);
    channel_config_set_dreq(&channel->dma.rx, pio_get_dreq(pio, channel->sm, false));
    channel_config_set_read_increment(&channel->dma.rx, false);
    channel_config_set_write_increment(&channel->dma.rx, true);

    channel->dma.tx = dma_channel_get_default_config(channel->dma.id);
    channel_config_set_transfer_data_size(&channel->dma.tx, DMA_SIZE_8);
    channel_config_set_dreq(&channel->dma.tx, pio_get_dreq(pio, channel->sm, true));
    channel_config_set_read_increment(&channel->dma.tx, true);
    channel_config_set_write_increment(&channel->dma.tx, false);
}


void joybus_init (PIO pio, uint8_t channels, uint *pins, joybus_callback_t callback) {
    assert(!joybus);
    assert(channels > 0);
    assert(channels <= 4);
    assert(pins);
    assert(callback);

    joybus = malloc(sizeof(joybus_t));
    assert(joybus);
    joybus->pio = pio;
    joybus->offset = pio_add_program(joybus->pio, &joybus_program);
    joybus->callback = callback;
    joybus->channels = channels;
    joybus->channel = malloc(sizeof(joybus_channel_t) * joybus->channels);
    assert(joybus->channel);

    for (uint8_t ch = 0; ch < joybus->channels; ch += 1) {
        joybus->channel[ch].sm = pio_claim_unused_sm(joybus->pio, true);
        joybus_dma_init(joybus->pio, &joybus->channel[ch]);
        joybus_program_init(joybus->pio, joybus->channel[ch].sm, joybus->offset, pins[ch]);
        joybus_start_receive_data(ch);
    }

    joybus->irq.dma = DMA_IRQ_0;
    irq_add_shared_handler(joybus->irq.dma, joybus_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
    irq_set_priority(joybus->irq.dma, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(joybus->irq.dma, true);

    joybus->irq.pio = pio_get_index(joybus->pio) == 0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
    irq_add_shared_handler(joybus->irq.pio, joybus_pio_irq_handler, PICO_SHARED_IRQ_HANDLER_HIGHEST_ORDER_PRIORITY);
    irq_set_priority(joybus->irq.pio, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(joybus->irq.pio, true);
}

bool __time_critical_func(joybus_check_address_crc) (uint16_t address) {
    uint8_t crc = 0x00;
    for (int i = 0; i < 11; i += 1) {
        if (address & (1 << (15 - i))) {
            crc ^= joybus_address_crc_table[i];
        }
    }
    return (crc == (address & 0x1F));
}

uint8_t __time_critical_func(joybus_calculate_data_crc) (uint8_t *buffer) {
    uint8_t crc = 0x00;
    for (uint8_t byte = 0; byte < 32; byte += 1) {
        crc = joybus_data_crc_table[crc ^ buffer[byte]];
    }
    return crc;
}

static uint8_t __time_critical_func(joybus_callback) (uint8_t ch, uint8_t cmd, uint8_t rx_length, uint8_t *rx_buffer, uint8_t *tx_buffer) {
    uint8_t tx_length = 0;

    if (ch > 0) {
        return 0;
    }

    switch (cmd) {
        case JOYBUS_CMD_INFO:
            if (rx_length == 0) {
                // uint32_t info = 0x050002;
                uint32_t info = 0x008000;
                tx_length = 3;
                tx_buffer[0] = ((info >> 16) & 0xFF);
                tx_buffer[1] = ((info >> 8) & 0xFF);
                tx_buffer[2] = (info & 0xFF);
            }
            break;

        // case JOYBUS_CMD_STATE:
        //     if (rx_length == 0) {
        //         uint32_t state = 0;
        //         queue_try_remove(&joybus_read_queue, &state);
        //         tx_length = 4;
        //         tx_buffer[0] = ((state >> 24) & 0xFF);
        //         tx_buffer[1] = ((state >> 16) & 0xFF);
        //         tx_buffer[2] = ((state >> 8) & 0xFF);
        //         tx_buffer[3] = (state & 0xFF);
        //         queue_try_add(&joybus_write_queue, &state);
        //     }
        //     break;

        case JOYBUS_CMD_EEPROM_READ:
        
            if (rx_length != 0) {
                uint8_t blockToRead = rx_buffer[0];
                tx_length = 8;
                tx_buffer[0] = 0xA0;
                tx_buffer[1] = 0xA1;
                tx_buffer[2] = 0xA2;
                tx_buffer[3] = 0xA3;
                tx_buffer[4] = 0xA4;
                tx_buffer[5] = 0xA5;
                tx_buffer[6] = 0xA6;
                tx_buffer[7] = 0xA7;
            }
            break;

        case JOYBUS_CMD_EEPROM_WRITE:
            if (rx_length != 0) {
                uint8_t blockToWrite = rx_buffer[0];
                // 8 bytes of data
                for(int i = 0; i < 8; i++) {
                    eeprom[blockToWrite+i] = rx_buffer[i];
                }
                tx_length = 1;
                tx_buffer[0] = 0x00;
            }
            break;

        case JOYBUS_CMD_RESET:
            // if (rx_length == 0) {
            //     uint32_t state = 0;
            //     queue_try_peek(&joybus_read_queue, &state);
            //     tx_length = 4;
            //     tx_buffer[0] = ((state >> 24) & 0xFF);
            //     tx_buffer[1] = ((state >> 16) & 0xFF);
            //     tx_buffer[2] = ((state >> 8) & 0xFF);
            //     tx_buffer[3] = (state & 0xFF);
            // }
            break;
    }

    return tx_length;
}

void initJoybus() {
    uint joybus_pins[1] = { 21 };
    joybus_init(pio1, 1, joybus_pins, joybus_callback);
}

static bool joybusHasInitialInit = false;
void enable_joybus() {
    initJoybus();
}

void disable_joybus() {
    pio_sm_set_enabled(joybus->pio, joybus->channel[0].sm, false);
    pio_remove_program(joybus->pio, &joybus_program, joybus->offset);
}

void change_eeprom_type(uint16_t newType) {
    if (newType != EEPROM_TYPE_4K || newType != EEPROM_TYPE_16K) {
        eeprom_type = EEPROM_TYPE_4K; // default to 4kb
    } else {
        eeprom_type = newType;
    }
}