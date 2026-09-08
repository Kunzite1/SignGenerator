#ifndef APP_UI_H
#define APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef struct {
    const char *waveform_name;
    uint32_t set_frequency_hz;
    uint32_t actual_frequency_millihz;
    bool running;
} app_ui_state_t;

/**
 * @brief Initialize the OLED on I2C1 at the normal 7-bit address 0x3C.
 *
 * Call this once after MX_I2C1_Init(). A missing display is reported through
 * the return value and does not require the rest of the application to stop.
 */
HAL_StatusTypeDef app_ui_init(void);

/**
 * @brief Redraw the display if its state has changed.
 */
HAL_StatusTypeDef app_ui_render(const app_ui_state_t *state);

bool app_ui_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UI_H */
