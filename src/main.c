#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "usb_descriptor.h"

#define I2S_PORT I2S_NUM_0
#define I2S_READ_BUFFER_BYTES 512

static const char *TAG = "INMP441";

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[AUDIO_IN_PACKET];

volatile bool tx_flag = 0;
StreamBufferHandle_t audio_stream_buf = NULL;
volatile uint32_t s_mic_sample_rate = AUDIO_IN_MAX_FREQ;

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
  switch (event)
  {
  case USBD_EVENT_RESET:
    break;
  case USBD_EVENT_CONNECTED:
    break;
  case USBD_EVENT_DISCONNECTED:
    break;
  case USBD_EVENT_RESUME:
    break;
  case USBD_EVENT_SUSPEND:
    break;
  case USBD_EVENT_CONFIGURED:
    break;
  case USBD_EVENT_SET_REMOTE_WAKEUP:
    break;
  case USBD_EVENT_CLR_REMOTE_WAKEUP:
    break;
  default:
    break;
  }
}

void usbd_audio_open(uint8_t busid, uint8_t intf)
{
  tx_flag = 1;
  USB_LOG_RAW("OPEN\r\n");

  if (audio_stream_buf != NULL) {
    xStreamBufferReset(audio_stream_buf);
  }

  uint32_t packet_size = (s_mic_sample_rate * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
  if (packet_size > AUDIO_IN_PACKET) {
    packet_size = AUDIO_IN_PACKET;
  }

  memset(write_buffer, 0, packet_size);
  usbd_ep_start_write(busid, AUDIO_IN_EP, write_buffer, packet_size);
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
  USB_LOG_RAW("CLOSE\r\n");
  tx_flag = 0;
}
volatile int current_mic_volume = 100; 

void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume) {
    current_mic_volume = volume;
    // 你可以把 current_mic_volume 乘進去你的 sinf() 計算裡來動態改變正弦波大小
}

int usbd_audio_get_volume(uint8_t busid, uint8_t ep, uint8_t ch) {
    return current_mic_volume;
}

void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute) {

}

bool usbd_audio_get_mute(uint8_t busid, uint8_t ep, uint8_t ch) {
    return false;
}
void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq)
{
  if (ep == AUDIO_IN_EP)
  {
    s_mic_sample_rate = sampling_freq;
  }
}

uint32_t usbd_audio_get_sampling_freq(uint8_t busid, uint8_t ep)
{
  (void)busid;

  uint32_t freq = 0;

  if (ep == AUDIO_IN_EP)
  {
    freq = s_mic_sample_rate;
  }

  return freq;
}

void usbd_audio_get_sampling_freq_table(uint8_t busid, uint8_t ep, uint8_t **sampling_freq_table)
{
  if (ep == AUDIO_IN_EP)
  {
    *sampling_freq_table = (uint8_t *)mic_default_sampling_freq_table;
  }
}

void usbd_audio_iso_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
  if (!tx_flag) {
    return;
  }

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  uint32_t packet_size = (s_mic_sample_rate * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
  if (packet_size > AUDIO_IN_PACKET) {
    packet_size = AUDIO_IN_PACKET;
  }

  size_t received = 0;
  if (audio_stream_buf != NULL) {
    received = xStreamBufferReceiveFromISR(audio_stream_buf, write_buffer, packet_size, &xHigherPriorityTaskWoken);
  }

  if (received < packet_size) {
    memset(write_buffer + received, 0, packet_size - received);
  }

  usbd_ep_start_write(busid, AUDIO_IN_EP, write_buffer, packet_size);

  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

static struct usbd_endpoint audio_in_ep = {
    .ep_cb = usbd_audio_iso_in_callback,
    .ep_addr = AUDIO_IN_EP};

struct usbd_interface intf0;
struct usbd_interface intf1;

struct audio_entity_info audio_entity_table[] = {
    {.bEntityId = AUDIO_IN_FU_ID,
     .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
     .ep = AUDIO_IN_EP},
};

void audio_v2_init(uint8_t busid, uintptr_t reg_base)
{
  usbd_desc_register(busid, &audio_v1_descriptor);
  usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf0, 0x0100, audio_entity_table, 1));
  usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf1, 0x0100, audio_entity_table, 1));
  usbd_add_endpoint(busid, &audio_in_ep);

  audio_stream_buf = xStreamBufferCreate(AUDIO_IN_PACKET * 10, AUDIO_IN_PACKET);
  if (audio_stream_buf == NULL)
  {
    ESP_LOGE("USB_AUDIO_DEMO", "Failed to create stream buffer");
  }

  int ret = usbd_initialize(busid, reg_base, usbd_event_handler);
  while (ret != 0)
  {
    ESP_LOGE("USB_AUDIO_DEMO", "usbd_initialize failed: %d", ret);
  }
}

#define I2S_SAMPLE_RATE AUDIO_IN_MAX_FREQ

static esp_err_t i2s_init(void)
{
  i2s_config_t i2s_config = {
      .mode = I2S_MODE_MASTER | I2S_MODE_RX,
      .sample_rate = I2S_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 128,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "i2s_driver_install failed: %s", esp_err_to_name(err));
    return err;
  }

  i2s_pin_config_t pin_config = {
      .bck_io_num = 36,
      .ws_io_num = 35,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = 37,
  };

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "i2s_set_pin failed: %s", esp_err_to_name(err));
    return err;
  }

  err = i2s_zero_dma_buffer(I2S_PORT);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "i2s_zero_dma_buffer failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "INMP441 I2S init done: SD=37, SCK=36, WS=35, SR=%u", I2S_SAMPLE_RATE);
  return ESP_OK;
}

void i2s_peak_task(void *arg)
{
  const size_t read_size = 256 * sizeof(int32_t);
  int32_t read_buf[256];
  uint32_t max_amplitude = 0;
  uint32_t frame_count = 0;

  while (1)
  {
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, read_buf, read_size, &bytes_read, portMAX_DELAY);
    if (err != ESP_OK || bytes_read == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    int num_samples = bytes_read / sizeof(int32_t);
    for (int i = 0; i < num_samples; i++)
    {
      int16_t sample16 = (int16_t)(read_buf[i] >> 16);
      uint32_t abs_val = sample16 == INT16_MIN ? INT16_MAX : (uint32_t)(sample16 < 0 ? -sample16 : sample16);
      if (abs_val > max_amplitude)
      {
        max_amplitude = abs_val;
      }
    }

    frame_count += num_samples / 2;
    if (frame_count >= I2S_SAMPLE_RATE)
    {
      ESP_LOGI(TAG, "1s max amplitude = %u", max_amplitude);
      max_amplitude = 0;
      frame_count = 0;
    }
  }
}

float g_phase[IN_CHANNEL_NUM] = {0};

void audio_v2_task(void *arg)
{
  const uint8_t busid = 0;
  int16_t pcm_local_buf[AUDIO_IN_PACKET / HALF_WORD_BYTES];
  int32_t i2s_read_buf[AUDIO_IN_PACKET / sizeof(int32_t)];
  uint32_t max_amplitude = 0;
  uint32_t frame_count = 0;

  while (1)
  {
    uint32_t packet_size = (s_mic_sample_rate * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
    if (packet_size > AUDIO_IN_PACKET)
    {
      packet_size = AUDIO_IN_PACKET;
    }

    const int frame_count_per_packet = packet_size / (HALF_WORD_BYTES * IN_CHANNEL_NUM);
    size_t read_size = frame_count_per_packet * 2 * sizeof(int32_t);
    if (read_size > sizeof(i2s_read_buf))
    {
      read_size = sizeof(i2s_read_buf);
    }

    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, i2s_read_buf, read_size, &bytes_read, portMAX_DELAY);
    if (err != ESP_OK || bytes_read == 0)
    {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    int samples_read = bytes_read / sizeof(int32_t);
    int frames_to_fill = frame_count_per_packet;
    int available_frames = samples_read / 2;
    if (available_frames < frames_to_fill)
    {
      frames_to_fill = available_frames;
    }

    for (int frame = 0; frame < frames_to_fill; frame++)
    {
      int32_t sample_r = i2s_read_buf[frame * 2];
      int32_t sample_l = i2s_read_buf[frame * 2 + 1];
      uint32_t abs_r = sample_r == INT32_MIN ? INT32_MAX : (uint32_t)(sample_r < 0 ? -sample_r : sample_r);
      uint32_t abs_l = sample_l == INT32_MIN ? INT32_MAX : (uint32_t)(sample_l < 0 ? -sample_l : sample_l);
      int32_t sample32 = abs_r >= abs_l ? sample_r : sample_l;
      int16_t sample16 = (int16_t)(sample32 >> 16);
      uint32_t abs_val = sample16 == INT16_MIN ? INT16_MAX : (uint32_t)(sample16 < 0 ? -sample16 : sample16);
      if (abs_val > max_amplitude)
      {
        max_amplitude = abs_val;
      }
      frame_count++;

      int idx = frame * IN_CHANNEL_NUM;
      for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
      {
        pcm_local_buf[idx++] = sample16;
      }
    }

    if (frames_to_fill < frame_count_per_packet)
    {
      int start = frames_to_fill * IN_CHANNEL_NUM;
      int count = (frame_count_per_packet - frames_to_fill) * IN_CHANNEL_NUM;
      memset(&pcm_local_buf[start], 0, count * sizeof(int16_t));
    }

    if (usb_device_is_configured(busid) && tx_flag && audio_stream_buf != NULL)
    {
      xStreamBufferSend(audio_stream_buf, pcm_local_buf, packet_size, portMAX_DELAY);
    }

    if (frame_count >= s_mic_sample_rate)
    {
      max_amplitude = 0;
      frame_count = 0;
    }
  }
}

void app_main()
{
  const uint8_t busid = 0;
  esp_err_t err = i2s_init();
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "I2S initialization failed");
  }

  audio_v2_init(busid, ESP_USBD_BASE);
  xTaskCreate(audio_v2_task, "audio_v2_task", 4096, NULL, 5, NULL);
  while (true)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}