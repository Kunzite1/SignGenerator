#include "app_serial.h"

#include <stdbool.h>

#include "usart.h"

/* Keep firmware console output ASCII: the host script decodes it as-is. */
static const char serial_hint[] =
    "\r\nconsole: q=start/stop w=next-wave e=freq- r=freq+\r\n";

static bool initialized;

/*
 * USART1 is shared with the status log written by app_report_status(), so this
 * module touches the receive path only. CubeMX leaves the USART1 global
 * interrupt disabled (stm32f1xx_it.c has no USART1_IRQHandler), so the loop
 * polls RXNE instead of using the interrupt-driven HAL API. That is safe here:
 * the main loop runs every ~1 ms while one byte lasts ~87 us at 115200 baud,
 * and neither a human nor tools/serial_console.py sends back-to-back bytes.
 */
static app_button_event_t serial_decode(uint8_t byte)
{
    switch (byte)
    {
        case 'q':
        case 'Q':
            return APP_BUTTON_EVENT_START_STOP;

        case 'w':
        case 'W':
            return APP_BUTTON_EVENT_WAVE_NEXT;

        case 'e':
        case 'E':
            return APP_BUTTON_EVENT_FREQ_DOWN;

        case 'r':
        case 'R':
            return APP_BUTTON_EVENT_FREQ_UP;

        default:
            return APP_BUTTON_EVENT_NONE;
    }
}

void app_serial_init(void)
{
    /* Drop anything the host typed while the firmware was still starting. */
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        (void)huart1.Instance->DR;
    }

    initialized = true;

    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)serial_hint,
        (uint16_t)(sizeof(serial_hint) - 1U),
        50U);
}

app_button_event_t app_serial_poll(void)
{
    app_button_event_t events = APP_BUTTON_EVENT_NONE;

    if (!initialized)
    {
        return APP_BUTTON_EVENT_NONE;
    }

    /*
     * A byte lost to overrun cannot be recovered. Clearing the flag reads SR
     * followed by DR as the F1 reference manual requires, which discards at
     * most one more byte; doing it first also lets the drain loop below rely on
     * RXNE alone.
     */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }

    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)(huart1.Instance->DR & 0xFFU);

        events = (app_button_event_t)(events | serial_decode(byte));
    }

    return events;
}
