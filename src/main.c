#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include "mic_i2s.h"
#include "mic_sim.h"
#include "custom_usb_audio.h"

#define USE_I2S_MIC

static const char *TAG = "APP_MAIN";
static QueueHandle_t audio_ready_queue = NULL;
static QueueHandle_t audio_free_queue = NULL;

static bool create_audio_queues(void)
{
  audio_ready_queue = xQueueCreate(AUDIO_TRANSFER_BUFFER_COUNT, sizeof(uint8_t *));
  audio_free_queue = xQueueCreate(AUDIO_TRANSFER_BUFFER_COUNT, sizeof(uint8_t *));
  if (audio_ready_queue == NULL || audio_free_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create audio queues");
    return false;
  }
  return true;
}

void app_main(void)
{
  esp_err_t err;
  if (!create_audio_queues())
  {
    ESP_LOGE(TAG, "Failed to create audio queues");
    return;
  }

  err = usb_audio_init(audio_ready_queue, audio_free_queue);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "USB mic initialization failed");
    return;
  }

#ifdef USE_I2S_MIC
  ESP_LOGI(TAG, "Using I2S microphone");
  err = mic_i2s_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "I2S initialization failed");
    return;
  }
  mic_i2s_start_task(audio_free_queue, audio_ready_queue);
#else
  ESP_LOGI(TAG, "Using simulated microphone");
  const uint32_t sampling_freq_table[4] = {100, 1000, 4000, 10000};
  mic_sim_start_task(audio_free_queue, audio_ready_queue, sampling_freq_table);
#endif
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}