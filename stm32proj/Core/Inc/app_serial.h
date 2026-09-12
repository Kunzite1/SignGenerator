#ifndef APP_SERIAL_H
#define APP_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_buttons.h"

/**
 * @brief Prepare USART1 for single-character console commands.
 *
 * Call this once after MX_USART1_UART_Init(). Stale receive state is dropped
 * and a one-line key hint is written to the same port that carries the status
 * log, so a plain terminal (picocom, minicom) works without extra setup.
 */
void app_serial_init(void);

/**
 * @brief Return the console keys received since the previous call.
 *
 * Call this on every main-loop iteration and merge the result with the bits
 * from app_buttons_poll(); console keys reuse the button event bits because
 * they select the same four application actions. Unrecognized bytes are
 * discarded, and one event is produced per received key, so the host
 * terminal's own key repeat drives repeated steps of e and r.
 */
app_button_event_t app_serial_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_H */
