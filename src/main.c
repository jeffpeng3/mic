#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include "mic_i2s.h"
#include "mic_sim.h"
#include "custom_usb_audio.h"

#define STREAM_BUFFER_PACKET_COUNT 4
#define STREAM_BUFFER_SIZE (AUDIO_IN_PACKET * STREAM_BUFFER_PACKET_COUNT)

// #define USE_I2S_MIC

static const char *TAG = "APP_MAIN";
static StreamBufferHandle_t audio_buffer = NULL;

static bool create_audio_buffer(void)
{
  audio_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE, 1);
  if (audio_buffer == NULL)
  {
    ESP_LOGE(TAG, "Failed to create audio buffer");
    return false;
  }
  return true;
}

void app_main(void)
{
  esp_err_t err;
  if (!create_audio_buffer())
  {
    ESP_LOGE(TAG, "Failed to create audio buffer");
    return;
  }

  err = usb_audio_init(audio_buffer);
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
  mic_i2s_start_task(audio_buffer);
#else
  ESP_LOGI(TAG, "Using simulated microphone");
  const uint32_t sampling_freq_table[4] = {100, 1000, 4000, 10000};
  mic_sim_start_task(audio_buffer, sampling_freq_table);
#endif
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}