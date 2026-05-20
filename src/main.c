#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include <mic_i2s.h>
#include "custom_usb_audio.h"

#define STREAM_BUFFER_PACKET_COUNT 4
#define STREAM_BUFFER_SIZE (AUDIO_IN_PACKET * STREAM_BUFFER_PACKET_COUNT)

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
  esp_err_t err = mic_i2s_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "I2S initialization failed");
    return;
  }

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

  mic_i2s_start_task(audio_buffer);
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}