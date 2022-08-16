#pragma once

void ws2812b_write(int gpio, const uint8_t * pixels, uint32_t num_bytes);
void ws2812b_write_rgb(int gpio, uint8_t r, uint8_t g, uint8_t b);
