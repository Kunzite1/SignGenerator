#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    APP_BUTTON_EVENT_NONE = 0U,
    APP_BUTTON_EVENT_START_STOP = (1U << 0),
    APP_BUTTON_EVENT_WAVE_NEXT = (1U << 1),
    APP_BUTTON_EVENT_FREQ_DOWN = (1U << 2),
    APP_BUTTON_EVENT_FREQ_UP = (1U << 3)
} app_button_event_t;

/**
 * @brief Capture the initial key levels without generating key events.
 *
 * Call this once after MX_GPIO_Init().
 */
void app_buttons_init(void);

/**
 * @brief Scan and debounce all keys without blocking.
 *
 * Call this function on every main-loop iteration. It performs a GPIO scan at
 * most once every 10 ms and returns a bit mask of newly generated events.
 * The onboard and external function keys share events. Frequency keys repeat
 * while held; the other keys generate one event per physical press.
 */
app_button_event_t app_buttons_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BUTTONS_H */
