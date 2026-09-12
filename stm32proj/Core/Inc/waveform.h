#ifndef WAVEFORM_H
#define WAVEFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

#define WAVEFORM_MAX_SAMPLE_COUNT      256U
#define WAVEFORM_MIN_SAMPLE_COUNT      64U
#define WAVEFORM_MAX_SAMPLE_RATE_HZ    1000000U
#define WAVEFORM_MIN_FREQUENCY_HZ      1U
#define WAVEFORM_MAX_FREQUENCY_HZ      10000U
#define WAVEFORM_DEFAULT_FREQUENCY_HZ  1000U

/*
 * Noise sample rate, which is also its bandwidth (f_s / 2). The ceiling is set
 * by the half-buffer refill cost, not by the DAC: see waveform_fill_noise().
 */
#define WAVEFORM_NOISE_MIN_RATE_HZ      20000U
#define WAVEFORM_NOISE_MAX_RATE_HZ      500000U
#define WAVEFORM_NOISE_DEFAULT_RATE_HZ  100000U

typedef enum
{
    WAVEFORM_SINE = 0,
    WAVEFORM_SQUARE,
    WAVEFORM_TRIANGLE,
    WAVEFORM_SAWTOOTH,
    WAVEFORM_NOISE,
    WAVEFORM_TYPE_COUNT
} waveform_type_t;

/**
 * @brief Initialize the waveform state and TIM6 divider.
 *
 * Call this once after MX_DAC_Init(), MX_DMA_Init(), and MX_TIM6_Init().
 * Output remains stopped until waveform_start() is called.
 */
HAL_StatusTypeDef waveform_init(void);

/** Start or stop TIM6-triggered circular DAC DMA output. */
HAL_StatusTypeDef waveform_start(void);
HAL_StatusTypeDef waveform_stop(void);
HAL_StatusTypeDef waveform_toggle(void);

/** Select a waveform, or advance to the next waveform in the enum. */
HAL_StatusTypeDef waveform_set_type(waveform_type_t type);
HAL_StatusTypeDef waveform_next_type(void);

/**
 * @brief Set or adjust the requested output frequency.
 *
 * waveform_set_frequency() rejects values outside 1..10000 Hz, and rejects any
 * call while the noise type is selected: noise is described by its sample rate
 * instead (see waveform_set_sample_rate()).
 * waveform_step_frequency() saturates at those limits.
 */
HAL_StatusTypeDef waveform_set_frequency(uint32_t frequency_hz);
HAL_StatusTypeDef waveform_step_frequency(int32_t step_hz);

/**
 * @brief Set the noise sample rate, in Hz. Noise type only.
 *
 * Also the noise bandwidth, which is half the sample rate. Rejects values
 * outside WAVEFORM_NOISE_MIN_RATE_HZ..WAVEFORM_NOISE_MAX_RATE_HZ and any call
 * while another type is selected.
 */
HAL_StatusTypeDef waveform_set_sample_rate(uint32_t sample_rate_hz);
uint32_t waveform_get_sample_rate(void);
uint32_t waveform_get_actual_sample_rate_millihz(void);

bool waveform_is_running(void);
waveform_type_t waveform_get_type(void);
const char *waveform_type_name(waveform_type_t type);

/**
 * @brief The requested and achieved output frequency of the periodic types.
 *
 * Both read the periodic state, which is retained while the noise type is
 * selected; callers that display one or the other must branch on the type and
 * use waveform_get_sample_rate() and waveform_get_actual_sample_rate_millihz()
 * for noise.
 */
uint32_t waveform_get_frequency(void);
uint32_t waveform_get_actual_frequency(void);
uint32_t waveform_get_actual_frequency_millihz(void);

/** The active 12-bit sample table, useful for plotting a preview on the OLED. */
const uint16_t *waveform_get_samples(void);
uint32_t waveform_get_sample_count(void);

#ifdef __cplusplus
}
#endif

#endif /* WAVEFORM_H */
