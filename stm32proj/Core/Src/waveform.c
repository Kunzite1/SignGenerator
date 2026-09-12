#include "waveform.h"

#include <stddef.h>

#include "dac.h"
#include "tim.h"

#define WAVEFORM_DAC_MIN_VALUE   256U
#define WAVEFORM_DAC_MAX_VALUE   3839U
#define WAVEFORM_DAC_CODE_MAX    4095U
#define WAVEFORM_DAC_SPAN        \
    (WAVEFORM_DAC_MAX_VALUE - WAVEFORM_DAC_MIN_VALUE)
#define TIMER_DIVIDER_MAX        65536ULL

/*
 * Noise source. The seed is fixed on purpose: a captured trace can be
 * reproduced. xorshift32 never reaches zero from a non-zero state, so the
 * sequence cannot collapse.
 */
#define WAVEFORM_NOISE_SEED       0x2545F491U
#define WAVEFORM_NOISE_HALF_COUNT (WAVEFORM_MAX_SAMPLE_COUNT / 2U)

/*
 * One normalized cycle of an offset-binary sine wave. Values are rescaled
 * before output; keeping the lookup table in flash avoids target-side libm.
 */
static const uint16_t sine_samples[WAVEFORM_MAX_SAMPLE_COUNT] =
{
    2048U, 2098U, 2148U, 2198U, 2248U, 2298U, 2348U, 2398U,
    2447U, 2496U, 2545U, 2594U, 2642U, 2690U, 2737U, 2784U,
    2831U, 2877U, 2923U, 2968U, 3013U, 3057U, 3100U, 3143U,
    3185U, 3226U, 3267U, 3307U, 3346U, 3385U, 3423U, 3459U,
    3495U, 3530U, 3565U, 3598U, 3630U, 3662U, 3692U, 3722U,
    3750U, 3777U, 3804U, 3829U, 3853U, 3876U, 3898U, 3919U,
    3939U, 3958U, 3975U, 3992U, 4007U, 4021U, 4034U, 4045U,
    4056U, 4065U, 4073U, 4080U, 4085U, 4089U, 4093U, 4094U,
    4095U, 4094U, 4093U, 4089U, 4085U, 4080U, 4073U, 4065U,
    4056U, 4045U, 4034U, 4021U, 4007U, 3992U, 3975U, 3958U,
    3939U, 3919U, 3898U, 3876U, 3853U, 3829U, 3804U, 3777U,
    3750U, 3722U, 3692U, 3662U, 3630U, 3598U, 3565U, 3530U,
    3495U, 3459U, 3423U, 3385U, 3346U, 3307U, 3267U, 3226U,
    3185U, 3143U, 3100U, 3057U, 3013U, 2968U, 2923U, 2877U,
    2831U, 2784U, 2737U, 2690U, 2642U, 2594U, 2545U, 2496U,
    2447U, 2398U, 2348U, 2298U, 2248U, 2198U, 2148U, 2098U,
    2048U, 1997U, 1947U, 1897U, 1847U, 1797U, 1747U, 1697U,
    1648U, 1599U, 1550U, 1501U, 1453U, 1405U, 1358U, 1311U,
    1264U, 1218U, 1172U, 1127U, 1082U, 1038U, 995U,  952U,
    910U,  869U,  828U,  788U,  749U,  710U,  672U,  636U,
    600U,  565U,  530U,  497U,  465U,  433U,  403U,  373U,
    345U,  318U,  291U,  266U,  242U,  219U,  197U,  176U,
    156U,  137U,  120U,  103U,  88U,   74U,   61U,   50U,
    39U,   30U,   22U,   15U,   10U,   6U,    2U,    1U,
    0U,    1U,    2U,    6U,    10U,   15U,   22U,   30U,
    39U,   50U,   61U,   74U,   88U,   103U,  120U,  137U,
    156U,  176U,  197U,  219U,  242U,  266U,  291U,  318U,
    345U,  373U,  403U,  433U,  465U,  497U,  530U,  565U,
    600U,  636U,  672U,  710U,  749U,  788U,  828U,  869U,
    910U,  952U,  995U,  1038U, 1082U, 1127U, 1172U, 1218U,
    1264U, 1311U, 1358U, 1405U, 1453U, 1501U, 1550U, 1599U,
    1648U, 1697U, 1747U, 1797U, 1847U, 1897U, 1947U, 1997U
};

static uint16_t waveform_samples[WAVEFORM_MAX_SAMPLE_COUNT]
    __attribute__((aligned(4)));
static waveform_type_t current_type = WAVEFORM_SINE;
static uint32_t requested_frequency_hz = WAVEFORM_DEFAULT_FREQUENCY_HZ;
static uint32_t actual_frequency_millihz;
static uint32_t active_sample_count = WAVEFORM_MAX_SAMPLE_COUNT;
static uint32_t timer_clock_hz;
static bool initialized;
static bool running;

static uint32_t noise_state = WAVEFORM_NOISE_SEED;
static uint32_t requested_sample_rate_hz = WAVEFORM_NOISE_DEFAULT_RATE_HZ;
static uint32_t actual_sample_rate_millihz;

/*
 * Set while the noise type is streaming, so the DMA half/full callbacks know
 * they own the sample table. Only ever changed with the output stopped.
 */
static volatile bool noise_active;

static uint32_t waveform_read_timer_clock(void)
{
    uint32_t clock_hz = HAL_RCC_GetPCLK1Freq();

    /* APB1 timers run at twice PCLK1 whenever its prescaler is not 1. */
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U)
    {
        clock_hz *= 2U;
    }

    return clock_hz;
}

static uint32_t waveform_select_sample_count(uint32_t frequency_hz)
{
    uint32_t sample_count = WAVEFORM_MAX_SAMPLE_COUNT;

    while ((sample_count > WAVEFORM_MIN_SAMPLE_COUNT)
           && (((uint64_t)frequency_hz * sample_count)
               > WAVEFORM_MAX_SAMPLE_RATE_HZ))
    {
        sample_count /= 2U;
    }

    return sample_count;
}

static uint16_t waveform_scale_sine_sample(uint16_t sample)
{
    uint32_t scaled = ((uint32_t)sample * WAVEFORM_DAC_SPAN)
                      + (WAVEFORM_DAC_CODE_MAX / 2U);

    return (uint16_t)(WAVEFORM_DAC_MIN_VALUE
                      + (scaled / WAVEFORM_DAC_CODE_MAX));
}

static void waveform_fill_samples(waveform_type_t type, uint32_t sample_count)
{
    uint32_t i;
    uint32_t sine_stride = WAVEFORM_MAX_SAMPLE_COUNT / sample_count;

    for (i = 0U; i < sample_count; ++i)
    {
        switch (type)
        {
            case WAVEFORM_SINE:
                waveform_samples[i] = waveform_scale_sine_sample(
                    sine_samples[i * sine_stride]);
                break;

            case WAVEFORM_SQUARE:
                waveform_samples[i] = (i < (sample_count / 2U))
                    ? WAVEFORM_DAC_MAX_VALUE
                    : WAVEFORM_DAC_MIN_VALUE;
                break;

            case WAVEFORM_TRIANGLE:
                if (i < (sample_count / 2U))
                {
                    waveform_samples[i] = (uint16_t)(
                        WAVEFORM_DAC_MIN_VALUE
                        + ((i * WAVEFORM_DAC_SPAN)
                           / (sample_count / 2U)));
                }
                else
                {
                    waveform_samples[i] = (uint16_t)(
                        WAVEFORM_DAC_MIN_VALUE
                        + (((sample_count - i) * WAVEFORM_DAC_SPAN)
                           / (sample_count / 2U)));
                }
                break;

            case WAVEFORM_SAWTOOTH:
                waveform_samples[i] = (uint16_t)(
                    WAVEFORM_DAC_MIN_VALUE
                    + ((i * WAVEFORM_DAC_SPAN) / (sample_count - 1U)));
                break;

            default:
                waveform_samples[i] = WAVEFORM_DAC_MIN_VALUE;
                break;
        }
    }
}

static uint32_t waveform_next_random(void)
{
    /* xorshift32: a handful of instructions, which is what the refill needs. */
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;

    return noise_state;
}

/**
 * @brief Replace count samples starting at first with fresh white noise.
 *
 * The codes stay inside the same window as the periodic waveforms. Scaling is
 * a multiply and a shift because the span (3584) is not a power of two and a
 * modulo would drag in __aeabi_uidiv -- Cortex-M3 has no hardware divide.
 * Clamping 12-bit values instead would pile the distribution up on both ends
 * of the window, which is not a flat noise distribution.
 */
static void waveform_fill_noise(uint32_t first, uint32_t count)
{
    uint32_t i;

    for (i = 0U; i < count; ++i)
    {
        uint32_t random = waveform_next_random();

        waveform_samples[first + i] = (uint16_t)(
            WAVEFORM_DAC_MIN_VALUE
            + (((random & 0xFFFFU) * WAVEFORM_DAC_SPAN) >> 16U));
    }
}

/*
 * Noise refill. The DAC streams the table circularly, so on a half/full
 * transfer the half that just finished is free until the DMA wraps back to it
 * N/2 samples later: that is the deadline, and refilling only the free half is
 * what stops the DMA from reading a half-written buffer. Refilling the whole
 * table on the completion interrupt alone would be too late, because the DMA
 * restarts at sample 0 immediately.
 *
 * These are HAL_DAC's weak callbacks; the interrupt plumbing itself lives in
 * stm32f1xx_it.c (DMA2_Channel3_IRQHandler) and dma.c (NVIC enabled).
 */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    (void)hdac;

    if (noise_active)
    {
        waveform_fill_noise(0U, WAVEFORM_NOISE_HALF_COUNT);
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
    (void)hdac;

    if (noise_active)
    {
        waveform_fill_noise(WAVEFORM_NOISE_HALF_COUNT, WAVEFORM_NOISE_HALF_COUNT);
    }
}

static uint32_t waveform_calculate_actual_millihz(
    uint64_t prescaler_divider,
    uint64_t period_divider)
{
    uint64_t total_divider = prescaler_divider * period_divider;

    return (uint32_t)(
        (((uint64_t)timer_clock_hz * 1000ULL)
         + (total_divider / 2ULL))
        / total_divider);
}

static uint64_t waveform_absolute_difference(uint64_t first, uint64_t second)
{
    return (first > second) ? (first - second) : (second - first);
}

/**
 * @brief Program TIM6 to trigger at update_rate_hz.
 *
 * Shared by both output modes: the periodic waveforms ask for
 * frequency x sample_count updates per second, the noise type asks for its
 * sample rate directly. The achieved rate is reported in millihz.
 */
static HAL_StatusTypeDef waveform_program_timer(
    uint32_t update_rate_hz,
    uint32_t *actual_millihz)
{
    uint64_t prescaler_divider;
    uint64_t period_divider;
    uint64_t alternate_period_divider;
    uint64_t period_denominator;
    uint64_t requested_millihz;
    uint32_t candidate_millihz;
    uint32_t alternate_millihz;

    if ((update_rate_hz == 0U) || (actual_millihz == NULL))
    {
        return HAL_ERROR;
    }

    /* Choose the smallest prescaler that lets the 16-bit ARR fit. */
    prescaler_divider =
        ((uint64_t)timer_clock_hz
         + ((uint64_t)update_rate_hz * TIMER_DIVIDER_MAX) - 1ULL)
        / ((uint64_t)update_rate_hz * TIMER_DIVIDER_MAX);

    if (prescaler_divider < 1ULL)
    {
        prescaler_divider = 1ULL;
    }
    if (prescaler_divider > TIMER_DIVIDER_MAX)
    {
        return HAL_ERROR;
    }

    /* Test both adjacent ARR values and keep the closest update rate. */
    period_denominator = (uint64_t)update_rate_hz * prescaler_divider;
    period_divider = (uint64_t)timer_clock_hz / period_denominator;

    if (period_divider < 1ULL)
    {
        period_divider = 1ULL;
    }
    if (period_divider > TIMER_DIVIDER_MAX)
    {
        period_divider = TIMER_DIVIDER_MAX;
    }

    alternate_period_divider = period_divider;
    if ((period_divider < TIMER_DIVIDER_MAX)
        && (((uint64_t)timer_clock_hz % period_denominator) != 0ULL))
    {
        alternate_period_divider = period_divider + 1ULL;
    }

    requested_millihz = (uint64_t)update_rate_hz * 1000ULL;
    candidate_millihz = waveform_calculate_actual_millihz(
        prescaler_divider,
        period_divider);
    alternate_millihz = waveform_calculate_actual_millihz(
        prescaler_divider,
        alternate_period_divider);

    if (waveform_absolute_difference(alternate_millihz, requested_millihz)
        < waveform_absolute_difference(candidate_millihz, requested_millihz))
    {
        period_divider = alternate_period_divider;
        candidate_millihz = alternate_millihz;
    }

    htim6.Init.Prescaler = (uint32_t)(prescaler_divider - 1ULL);
    htim6.Init.Period = (uint32_t)(period_divider - 1ULL);
    __HAL_TIM_SET_PRESCALER(&htim6, htim6.Init.Prescaler);
    __HAL_TIM_SET_AUTORELOAD(&htim6, htim6.Init.Period);
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    htim6.Instance->EGR = TIM_EVENTSOURCE_UPDATE;

    *actual_millihz = candidate_millihz;

    return HAL_OK;
}

/** Periodic types: one output cycle spans sample_count timer updates. */
static HAL_StatusTypeDef waveform_configure_timer(
    uint32_t frequency_hz,
    uint32_t sample_count)
{
    HAL_StatusTypeDef status;
    uint32_t timer_millihz;

    status = waveform_program_timer(
        frequency_hz * sample_count,
        &timer_millihz);

    if (status == HAL_OK)
    {
        actual_frequency_millihz = timer_millihz / sample_count;
    }

    return status;
}

/** Noise: the timer update rate is the sample rate itself. */
static HAL_StatusTypeDef waveform_configure_noise_timer(void)
{
    return waveform_program_timer(
        requested_sample_rate_hz,
        &actual_sample_rate_millihz);
}

HAL_StatusTypeDef waveform_init(void)
{
    HAL_StatusTypeDef status;

    if (initialized)
    {
        return HAL_OK;
    }

    if ((hdac.Instance != DAC)
        || (hdac.DMA_Handle1 == NULL)
        || (htim6.Instance != TIM6))
    {
        return HAL_ERROR;
    }

    timer_clock_hz = waveform_read_timer_clock();
    if (timer_clock_hz == 0U)
    {
        return HAL_ERROR;
    }

    active_sample_count = waveform_select_sample_count(
        requested_frequency_hz);
    waveform_fill_samples(current_type, active_sample_count);
    status = waveform_configure_timer(
        requested_frequency_hz,
        active_sample_count);
    if (status != HAL_OK)
    {
        return status;
    }

    running = false;
    initialized = true;
    return HAL_OK;
}

HAL_StatusTypeDef waveform_start(void)
{
    HAL_StatusTypeDef status;

    if (!initialized)
    {
        return HAL_ERROR;
    }
    if (running)
    {
        return HAL_OK;
    }

    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    status = HAL_DAC_SetValue(
        &hdac,
        DAC_CHANNEL_1,
        DAC_ALIGN_12B_R,
        waveform_samples[active_sample_count - 1U]);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_DAC_Start_DMA(
        &hdac,
        DAC_CHANNEL_1,
        (const uint32_t *)waveform_samples,
        active_sample_count,
        DAC_ALIGN_12B_R);
    if (status != HAL_OK)
    {
        return status;
    }

    /*
     * Noise regenerates each half buffer inside the transfer callbacks, so it
     * needs those interrupts; the static periodic tables must not get them.
     * HAL_DAC_Start_DMA() leaves them enabled, hence the explicit disable.
     */
    noise_active = (current_type == WAVEFORM_NOISE);
    if (noise_active)
    {
        __HAL_DMA_ENABLE_IT(hdac.DMA_Handle1, DMA_IT_HT | DMA_IT_TC);
    }
    else
    {
        __HAL_DMA_DISABLE_IT(hdac.DMA_Handle1, DMA_IT_HT | DMA_IT_TC);
    }

    status = HAL_TIM_Base_Start(&htim6);
    if (status != HAL_OK)
    {
        (void)HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
        return status;
    }

    running = true;
    return HAL_OK;
}

HAL_StatusTypeDef waveform_stop(void)
{
    HAL_StatusTypeDef status;

    if (!initialized)
    {
        return HAL_ERROR;
    }
    if (!running)
    {
        return HAL_OK;
    }

    /* Take the sample table back from the transfer callbacks first. */
    noise_active = false;
    __HAL_DMA_DISABLE_IT(hdac.DMA_Handle1, DMA_IT_HT | DMA_IT_TC);

    status = HAL_TIM_Base_Stop(&htim6);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    running = false;
    return status;
}

HAL_StatusTypeDef waveform_toggle(void)
{
    return running ? waveform_stop() : waveform_start();
}

HAL_StatusTypeDef waveform_set_type(waveform_type_t type)
{
    HAL_StatusTypeDef status;
    bool restart;

    if (!initialized || ((uint32_t)type >= (uint32_t)WAVEFORM_TYPE_COUNT))
    {
        return HAL_ERROR;
    }
    if (type == current_type)
    {
        return HAL_OK;
    }

    restart = running;
    if (restart)
    {
        status = waveform_stop();
        if (status != HAL_OK)
        {
            return status;
        }
    }

    /*
     * The two modes are programmed differently, so switching modes rebuilds
     * both the table and the timer: noise always uses the full table at its
     * sample rate, a periodic type gets its frequency-appropriate tier back.
     */
    if (type == WAVEFORM_NOISE)
    {
        active_sample_count = WAVEFORM_MAX_SAMPLE_COUNT;
        waveform_fill_noise(0U, active_sample_count);
        status = waveform_configure_noise_timer();
    }
    else
    {
        active_sample_count = waveform_select_sample_count(
            requested_frequency_hz);
        waveform_fill_samples(type, active_sample_count);
        status = waveform_configure_timer(
            requested_frequency_hz,
            active_sample_count);
    }

    if (status != HAL_OK)
    {
        return status;
    }

    current_type = type;

    return restart ? waveform_start() : HAL_OK;
}

HAL_StatusTypeDef waveform_next_type(void)
{
    waveform_type_t next_type;

    next_type = (waveform_type_t)(((uint32_t)current_type + 1U)
                                  % (uint32_t)WAVEFORM_TYPE_COUNT);
    return waveform_set_type(next_type);
}

HAL_StatusTypeDef waveform_set_frequency(uint32_t frequency_hz)
{
    HAL_StatusTypeDef status;
    uint32_t sample_count;
    bool restart;

    /* Noise is described by its sample rate; see waveform_set_sample_rate(). */
    if (!initialized
        || (current_type == WAVEFORM_NOISE)
        || (frequency_hz < WAVEFORM_MIN_FREQUENCY_HZ)
        || (frequency_hz > WAVEFORM_MAX_FREQUENCY_HZ))
    {
        return HAL_ERROR;
    }
    if (frequency_hz == requested_frequency_hz)
    {
        return HAL_OK;
    }

    restart = running;
    if (restart)
    {
        status = waveform_stop();
        if (status != HAL_OK)
        {
            return status;
        }
    }

    sample_count = waveform_select_sample_count(frequency_hz);
    if (sample_count != active_sample_count)
    {
        active_sample_count = sample_count;
        waveform_fill_samples(current_type, active_sample_count);
    }

    status = waveform_configure_timer(frequency_hz, active_sample_count);
    if (status != HAL_OK)
    {
        return status;
    }
    requested_frequency_hz = frequency_hz;

    return restart ? waveform_start() : HAL_OK;
}

HAL_StatusTypeDef waveform_step_frequency(int32_t step_hz)
{
    int64_t next_frequency_hz;

    if (!initialized)
    {
        return HAL_ERROR;
    }

    next_frequency_hz = (int64_t)requested_frequency_hz + step_hz;
    if (next_frequency_hz < WAVEFORM_MIN_FREQUENCY_HZ)
    {
        next_frequency_hz = WAVEFORM_MIN_FREQUENCY_HZ;
    }
    else if (next_frequency_hz > WAVEFORM_MAX_FREQUENCY_HZ)
    {
        next_frequency_hz = WAVEFORM_MAX_FREQUENCY_HZ;
    }

    return waveform_set_frequency((uint32_t)next_frequency_hz);
}

HAL_StatusTypeDef waveform_set_sample_rate(uint32_t sample_rate_hz)
{
    HAL_StatusTypeDef status;
    bool restart;

    if (!initialized
        || (current_type != WAVEFORM_NOISE)
        || (sample_rate_hz < WAVEFORM_NOISE_MIN_RATE_HZ)
        || (sample_rate_hz > WAVEFORM_NOISE_MAX_RATE_HZ))
    {
        return HAL_ERROR;
    }
    if (sample_rate_hz == requested_sample_rate_hz)
    {
        return HAL_OK;
    }

    restart = running;
    if (restart)
    {
        status = waveform_stop();
        if (status != HAL_OK)
        {
            return status;
        }
    }

    requested_sample_rate_hz = sample_rate_hz;

    /*
     * Only the trigger rate changes: the table keeps the noise it already
     * holds, it is just consumed at the new rate.
     */
    status = waveform_configure_noise_timer();
    if (status != HAL_OK)
    {
        return status;
    }

    return restart ? waveform_start() : HAL_OK;
}

uint32_t waveform_get_sample_rate(void)
{
    return requested_sample_rate_hz;
}

uint32_t waveform_get_actual_sample_rate_millihz(void)
{
    return actual_sample_rate_millihz;
}

bool waveform_is_running(void)
{
    return running;
}

waveform_type_t waveform_get_type(void)
{
    return current_type;
}

const char *waveform_type_name(waveform_type_t type)
{
    static const char *const names[WAVEFORM_TYPE_COUNT] =
    {
        "SINE",
        "SQUARE",
        "TRIANGLE",
        "SAWTOOTH",
        "NOISE"
    };

    return ((uint32_t)type < (uint32_t)WAVEFORM_TYPE_COUNT)
           ? names[type]
           : "UNKNOWN";
}

uint32_t waveform_get_frequency(void)
{
    return requested_frequency_hz;
}

uint32_t waveform_get_actual_frequency(void)
{
    return (actual_frequency_millihz + 500U) / 1000U;
}

uint32_t waveform_get_actual_frequency_millihz(void)
{
    return actual_frequency_millihz;
}

const uint16_t *waveform_get_samples(void)
{
    return waveform_samples;
}

uint32_t waveform_get_sample_count(void)
{
    return active_sample_count;
}
