/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>

#include "app_buttons.h"
#include "app_serial.h"
#include "app_ui.h"
#include "waveform.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DISPLAY_POWER_ON_DELAY_MS  50U
#define DISPLAY_RETRY_PERIOD_MS    2000U
#define HEARTBEAT_HALF_PERIOD_MS   500U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint32_t display_last_attempt_ms;
static uint32_t heartbeat_last_toggle_ms;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static int32_t app_get_frequency_step(bool increase);
static void app_handle_events(app_button_event_t events);
static void app_refresh_status(void);
static void app_report_status(void);
static void app_service_display(void);
static void app_service_heartbeat(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static int32_t app_get_frequency_step(bool increase)
{
  uint32_t frequency_hz = waveform_get_frequency();
  int32_t step_hz;

  if (increase)
  {
    if (frequency_hz < 10U)
    {
      step_hz = 1;
    }
    else if (frequency_hz < 100U)
    {
      step_hz = 10;
    }
    else if (frequency_hz < 1000U)
    {
      step_hz = 100;
    }
    else
    {
      step_hz = 1000;
    }
  }
  else
  {
    if (frequency_hz <= 10U)
    {
      step_hz = 1;
    }
    else if (frequency_hz <= 100U)
    {
      step_hz = 10;
    }
    else if (frequency_hz <= 1000U)
    {
      step_hz = 100;
    }
    else
    {
      step_hz = 1000;
    }
    step_hz = -step_hz;
  }

  return step_hz;
}

static void app_handle_events(app_button_event_t events)
{
  HAL_StatusTypeDef status = HAL_OK;

  if ((events & APP_BUTTON_EVENT_START_STOP) != 0U)
  {
    status = waveform_toggle();
  }
  if ((status == HAL_OK) && ((events & APP_BUTTON_EVENT_WAVE_NEXT) != 0U))
  {
    status = waveform_next_type();
  }

  /* Ignore contradictory frequency events caused by pressing both keys. */
  if ((status == HAL_OK)
      && ((events & (APP_BUTTON_EVENT_FREQ_DOWN | APP_BUTTON_EVENT_FREQ_UP))
          == APP_BUTTON_EVENT_FREQ_DOWN))
  {
    status = waveform_step_frequency(app_get_frequency_step(false));
  }
  else if ((status == HAL_OK)
           && ((events & (APP_BUTTON_EVENT_FREQ_DOWN | APP_BUTTON_EVENT_FREQ_UP))
               == APP_BUTTON_EVENT_FREQ_UP))
  {
    status = waveform_step_frequency(app_get_frequency_step(true));
  }

  if (status != HAL_OK)
  {
    Error_Handler();
  }
}

static void app_refresh_status(void)
{
  app_ui_state_t ui_state = {
    .waveform_name = waveform_type_name(waveform_get_type()),
    .set_frequency_hz = waveform_get_frequency(),
    .actual_frequency_millihz = waveform_get_actual_frequency_millihz(),
    .running = waveform_is_running()
  };

  (void)app_ui_render(&ui_state);
}

static void app_report_status(void)
{
  char message[96];
  uint32_t actual_millihz = waveform_get_actual_frequency_millihz();
  int length;

  length = snprintf(
      message,
      sizeof(message),
      "wave=%s set=%luHz actual=%lu.%03luHz points=%lu state=%s\r\n",
      waveform_type_name(waveform_get_type()),
      (unsigned long)waveform_get_frequency(),
      (unsigned long)(actual_millihz / 1000U),
      (unsigned long)(actual_millihz % 1000U),
      (unsigned long)waveform_get_sample_count(),
      waveform_is_running() ? "RUN" : "STOP");

  if (length > 0)
  {
    uint16_t transmit_length = (uint16_t)length;

    if ((size_t)length >= sizeof(message))
    {
      transmit_length = (uint16_t)(sizeof(message) - 1U);
    }
    (void)HAL_UART_Transmit(
        &huart1,
        (uint8_t *)message,
        transmit_length,
        50U);
  }
}

static void app_service_display(void)
{
  uint32_t now = HAL_GetTick();

  if (app_ui_is_ready()
      || ((uint32_t)(now - display_last_attempt_ms)
          < DISPLAY_RETRY_PERIOD_MS))
  {
    return;
  }

  display_last_attempt_ms = now;
  if (app_ui_init() == HAL_OK)
  {
    app_refresh_status();
  }
}

static void app_service_heartbeat(void)
{
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - heartbeat_last_toggle_ms)
      < HEARTBEAT_HALF_PERIOD_MS)
  {
    return;
  }

  heartbeat_last_toggle_ms = now;
  HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  app_buttons_init();
  app_serial_init();
  heartbeat_last_toggle_ms = HAL_GetTick();
  if (waveform_init() != HAL_OK)
  {
    Error_Handler();
  }
  if (waveform_start() != HAL_OK)
  {
    Error_Handler();
  }

  HAL_Delay(DISPLAY_POWER_ON_DELAY_MS);
  display_last_attempt_ms = HAL_GetTick();
  (void)app_ui_init();
  app_refresh_status();
  app_report_status();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    app_button_event_t events = app_buttons_poll();

    /* Console keys reuse the button events; app_serial.c currently disables them. */
    events = (app_button_event_t)(events | app_serial_poll());

    if (events != APP_BUTTON_EVENT_NONE)
    {
      app_handle_events(events);
      app_refresh_status();
      app_report_status();
    }

    app_service_display();
    app_service_heartbeat();
    HAL_Delay(1U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
