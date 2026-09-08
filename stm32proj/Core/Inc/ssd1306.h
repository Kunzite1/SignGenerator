#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define SSD1306_WIDTH       128U
#define SSD1306_HEIGHT      64U
#define SSD1306_I2C_ADDRESS 0x3CU

/**
 * @brief Initialize a 128x64 SSD1306 connected over I2C.
 * @param i2c HAL I2C peripheral handle.
 * @param address_7bit Unshifted 7-bit address, normally 0x3C.
 */
HAL_StatusTypeDef ssd1306_init(I2C_HandleTypeDef *i2c, uint8_t address_7bit);

void ssd1306_clear(bool pixel_on);
void ssd1306_draw_char(uint8_t x, uint8_t y, char character);
void ssd1306_draw_text(uint8_t x, uint8_t y, const char *text);
HAL_StatusTypeDef ssd1306_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
