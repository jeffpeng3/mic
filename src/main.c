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
#include "usb_descriptor.h"

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
float g_phase[IN_CHANNEL_NUM] = {0};

void audio_v2_task(void *arg)
{
  const uint8_t busid = 0;
  const float freqs[4] = {200.0f, 2000.0f, 10000.0f, 4000.0f};
  int16_t pcm_local_buf[AUDIO_IN_PACKET / HALF_WORD_BYTES];

  while (1)
  {
    if (!usb_device_is_configured(busid) || !tx_flag)
    {
      vTaskDelay(1);
      continue;
    }

    uint32_t packet_size = (s_mic_sample_rate * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
    if (packet_size > AUDIO_IN_PACKET)
    {
      packet_size = AUDIO_IN_PACKET;
    }

    const int frame_count = packet_size / (HALF_WORD_BYTES * IN_CHANNEL_NUM);
    int idx = 0;
    for (int frame = 0; frame < frame_count; frame++)
    {
      for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
      {
        float val = sinf(g_phase[ch]);
        pcm_local_buf[idx++] = (int16_t)(val * 32767.0f * (current_mic_volume / 100.0f));

        g_phase[ch] += 2.0f * M_PI * freqs[ch] / (float)s_mic_sample_rate;
        if (g_phase[ch] >= 2.0f * M_PI)
        {
          g_phase[ch] -= 2.0f * M_PI;
        }
      }
    }

    if (audio_stream_buf != NULL)
    {
      xStreamBufferSend(audio_stream_buf, pcm_local_buf, packet_size, portMAX_DELAY);
    }
  }
}

void app_main()
{
  const uint8_t busid = 0;
  audio_v2_init(busid, ESP_USBD_BASE);
  xTaskCreate(audio_v2_task, "audio_v2_task", 4096, NULL, 5, NULL);
  while (true)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}