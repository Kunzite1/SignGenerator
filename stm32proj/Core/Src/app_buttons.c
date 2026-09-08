#include "app_buttons.h"

#include <stdbool.h>
#include <stddef.h>

#include "main.h"

#define BUTTON_SCAN_PERIOD_MS       10U
#define BUTTON_DEBOUNCE_MS          30U
#define BUTTON_REPEAT_DELAY_MS      500U
#define BUTTON_REPEAT_INTERVAL_MS   150U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    app_button_event_t event;
    bool repeat_enabled;
    bool raw_pressed;
    bool stable_pressed;
    uint32_t raw_changed_at;
    uint32_t next_repeat_at;
} button_state_t;

static button_state_t buttons[] = {
    {
        KEY_START_STOP_GPIO_Port,
        KEY_START_STOP_Pin,
        APP_BUTTON_EVENT_START_STOP,
        false,
        false,
        false,
        0U,
        0U
    },
    {
        KEY_WAVE_GPIO_Port,
        KEY_WAVE_Pin,
        APP_BUTTON_EVENT_WAVE_NEXT,
        false,
        false,
        false,
        0U,
        0U
    },
    {
        KEY_FREQ_DOWN_GPIO_Port,
        KEY_FREQ_DOWN_Pin,
        APP_BUTTON_EVENT_FREQ_DOWN,
        true,
        false,
        false,
        0U,
        0U
    },
    {
        KEY_FREQ_UP_GPIO_Port,
        KEY_FREQ_UP_Pin,
        APP_BUTTON_EVENT_FREQ_UP,
        true,
        false,
        false,
        0U,
        0U
    }
};

static uint32_t last_scan_at;
static bool initialized;

static bool button_is_pressed(const button_state_t *button)
{
    return HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET;
}

static bool time_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void app_buttons_init(void)
{
    uint32_t now = HAL_GetTick();

    for (size_t index = 0U; index < (sizeof(buttons) / sizeof(buttons[0])); ++index) {
        bool pressed = button_is_pressed(&buttons[index]);

        buttons[index].raw_pressed = pressed;
        buttons[index].stable_pressed = pressed;
        buttons[index].raw_changed_at = now;
        buttons[index].next_repeat_at = now + BUTTON_REPEAT_DELAY_MS;
    }

    last_scan_at = now;
    initialized = true;
}

app_button_event_t app_buttons_poll(void)
{
    app_button_event_t events = APP_BUTTON_EVENT_NONE;
    uint32_t now;

    if (!initialized) {
        app_buttons_init();
        return APP_BUTTON_EVENT_NONE;
    }

    now = HAL_GetTick();
    if ((uint32_t)(now - last_scan_at) < BUTTON_SCAN_PERIOD_MS) {
        return APP_BUTTON_EVENT_NONE;
    }
    last_scan_at = now;

    for (size_t index = 0U; index < (sizeof(buttons) / sizeof(buttons[0])); ++index) {
        button_state_t *button = &buttons[index];
        bool pressed = button_is_pressed(button);

        if (pressed != button->raw_pressed) {
            button->raw_pressed = pressed;
            button->raw_changed_at = now;
        }

        if ((button->stable_pressed != button->raw_pressed)
            && ((uint32_t)(now - button->raw_changed_at) >= BUTTON_DEBOUNCE_MS)) {
            button->stable_pressed = button->raw_pressed;

            if (button->stable_pressed) {
                events = (app_button_event_t)(events | button->event);
                button->next_repeat_at = now + BUTTON_REPEAT_DELAY_MS;
            }
            continue;
        }

        if (button->repeat_enabled
            && button->stable_pressed
            && button->raw_pressed
            && time_reached(now, button->next_repeat_at)) {
            events = (app_button_event_t)(events | button->event);
            button->next_repeat_at = now + BUTTON_REPEAT_INTERVAL_MS;
        }
    }

    return events;
}
