/* hw_config.c
Copyright (c) 2022 Konrad Beckmann
Copyright (c) 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use 
this file except in compliance with the License. You may obtain a copy of the 
License at

   http://www.apache.org/licenses/LICENSE-2.0 
Unless required by applicable law or agreed to in writing, software distributed 
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR 
CONDITIONS OF ANY KIND, either express or implied. See the License for the 
specific language governing permissions and limitations under the License.
*/

#include <string.h>
//
#include "my_debug.h"
//
#include "hw_config.h"
//
#include "ff.h"					/* Obtains integer types */
//
#include "diskio.h"				/* Declarations of disk functions */

#include "pins_mcu2.h"

/*

PicoCart v2 pins:

Net     spi0    Pin#
--------------------
CLK     SCK        2
DAT0    MISO       4
CMD     MOSI       3
DAT3    CS         7
DAT1               5
DAT2               6

*/

void spi_dma_isr();

// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects.
static spi_t spis[] = {			// One for each SPI.
	{
	 .hw_inst = spi0,			// SPI component
	 .miso_gpio = PIN_SD_DAT0_UART1_TX,
	 .mosi_gpio = PIN_SD_CMD,
	 .sck_gpio = PIN_SD_CLK,
	 .set_drive_strength = true,
	 .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
	 .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,

	 // .baud_rate = 1000 * 1000,
	 //.baud_rate = 12500 * 1000,  // The limitation here is SPI slew rate.
	//  .baud_rate = 25 * 1000 * 1000,	// Actual frequency: 20833333. Has
	.baud_rate = 40 * 1000 * 1000,	// Actual frequency: 20833333. Has
	 // worked for me with SanDisk.        

	 .dma_isr = spi_dma_isr}
};

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = {	// One for each SD card
	{
	 .pcName = "0:",			// Name used to mount device
	 .spi = &spis[0],			// Pointer to the SPI driving this card
	 .ss_gpio = PIN_SD_DAT3,	// The SPI slave select GPIO for this SD card
	 .set_drive_strength = true,
	 .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
	 //.use_card_detect = false,        

	 .use_card_detect = false,
	 .card_detect_gpio = -1,	// Card detect
	 .card_detected_true = -1,	// What the GPIO read returns when a card is
	 // present. Use -1 if there is no card detect.

	 // State variables:
	 .m_Status = STA_NOINIT}
};

void spi_dma_isr()
{
	spi_irq_handler(&spis[0]);
}

/* ********************************************************************** */
size_t sd_get_num()
{
	return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num)
{
	if (num <= sd_get_num()) {
		return &sd_cards[num];
	} else {
		return NULL;
	}
}

size_t spi_get_num()
{
	return count_of(spis);
}

spi_t *spi_get_by_num(size_t num)
{
	if (num <= sd_get_num()) {
		return &spis[num];
	} else {
		return NULL;
	}
}

/* [] END OF FILE */
