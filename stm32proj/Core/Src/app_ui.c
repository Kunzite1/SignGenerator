#include "app_ui.h"

#include <stdio.h>
#include <string.h>

#include "i2c.h"
#include "ssd1306.h"

#define UI_WAVEFORM_NAME_SIZE 16U
#define UI_LINE_SIZE          24U

static bool ui_ready;
static bool previous_state_valid;
static uint32_t previous_set_frequency_hz;
static uint32_t previous_actual_frequency_millihz;
static bool previous_running;
static char previous_waveform_name[UI_WAVEFORM_NAME_SIZE];

static void copy_waveform_name(char *destination, const char *source)
{
    if (source == NULL) {
        source = "UNKNOWN";
    }

    (void)snprintf(destination, UI_WAVEFORM_NAME_SIZE, "%s", source);
}

HAL_StatusTypeDef app_ui_init(void)
{
    HAL_StatusTypeDef status;

    ui_ready = false;
    previous_state_valid = false;
    status = ssd1306_init(&hi2c1, SSD1306_I2C_ADDRESS);
    if (status == HAL_OK) {
        ui_ready = true;
    }

    return status;
}

HAL_StatusTypeDef app_ui_render(const app_ui_state_t *state)
{
    char waveform_name[UI_WAVEFORM_NAME_SIZE];
    char line[UI_LINE_SIZE];
    HAL_StatusTypeDef status;

    if (!ui_ready || (state == NULL)) {
        return HAL_ERROR;
    }

    copy_waveform_name(waveform_name, state->waveform_name);
    if (previous_state_valid
        && (state->set_frequency_hz == previous_set_frequency_hz)
        && (state->actual_frequency_millihz == previous_actual_frequency_millihz)
        && (state->running == previous_running)
        && (strncmp(
            waveform_name,
            previous_waveform_name,
            UI_WAVEFORM_NAME_SIZE
        ) == 0)) {
        return HAL_OK;
    }

    ssd1306_clear(false);

    (void)snprintf(line, sizeof(line), "WAVE : %s", waveform_name);
    ssd1306_draw_text(0U, 0U, line);

    (void)snprintf(
        line,
        sizeof(line),
        "SET  : %lu HZ",
        (unsigned long)state->set_frequency_hz
    );
    ssd1306_draw_text(0U, 16U, line);

    (void)snprintf(
        line,
        sizeof(line),
        "OUT  : %lu.%03lu HZ",
        (unsigned long)(state->actual_frequency_millihz / 1000U),
        (unsigned long)(state->actual_frequency_millihz % 1000U)
    );
    ssd1306_draw_text(0U, 32U, line);

    (void)snprintf(
        line,
        sizeof(line),
        "STATE: %s",
        state->running ? "RUN" : "STOP"
    );
    ssd1306_draw_text(0U, 48U, line);

    status = ssd1306_refresh();
    if (status != HAL_OK) {
        ui_ready = false;
        return status;
    }

    previous_set_frequency_hz = state->set_frequency_hz;
    previous_actual_frequency_millihz = state->actual_frequency_millihz;
    previous_running = state->running;
    memcpy(
        previous_waveform_name,
        waveform_name,
        sizeof(previous_waveform_name)
    );
    previous_state_valid = true;

    return HAL_OK;
}

bool app_ui_is_ready(void)
{
    return ui_ready;
}
