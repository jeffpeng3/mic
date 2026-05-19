#include <math.h>
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "usb_descriptor.h"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[AUDIO_IN_PACKET];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t feedback_buffer[AUDIO_IN_FEEDBACK_PACKET_SIZE];

volatile bool tx_flag = 0;
SemaphoreHandle_t ep_tx_done_sem = NULL;
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
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
  USB_LOG_RAW("CLOSE\r\n");
  tx_flag = 0;
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
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(ep_tx_done_sem, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void usbd_audio_feedback_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
  (void)busid;
  (void)ep;
  (void)nbytes;
  // log feedback callback, can be used to trigger next data generation if needed
  ESP_LOGI("USB_AUDIO_DEMO", "Feedback IN callback, nbytes: %d", nbytes);
}

static struct usbd_endpoint audio_in_ep = {
    .ep_cb = usbd_audio_iso_in_callback,
    .ep_addr = AUDIO_IN_EP};

static struct usbd_endpoint audio_in_feedback_ep = {
    .ep_cb = usbd_audio_feedback_in_callback,
    .ep_addr = AUDIO_IN_FEEDBACK_EP};

struct usbd_interface intf0;
struct usbd_interface intf1;

struct audio_entity_info audio_entity_table[] = {
    {.bEntityId = AUDIO_IN_CLOCK_ID,
     .bDescriptorSubtype = AUDIO_CONTROL_CLOCK_SOURCE,
     .ep = AUDIO_IN_EP},
    {.bEntityId = AUDIO_IN_FU_ID,
     .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
     .ep = AUDIO_IN_EP},
};

void audio_v2_init(uint8_t busid, uintptr_t reg_base)
{
  usbd_desc_register(busid, &audio_v2_descriptor);
  usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf0, 0x0200, audio_entity_table, 2));
  usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf1, 0x0200, audio_entity_table, 2));
  usbd_add_endpoint(busid, &audio_in_ep);
  usbd_add_endpoint(busid, &audio_in_feedback_ep);

  ep_tx_done_sem = xSemaphoreCreateBinary();
  if (ep_tx_done_sem == NULL)
  {
    ESP_LOGE("USB_AUDIO_DEMO", "Failed to create tx_done semaphore");
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
  const float freqs[IN_CHANNEL_NUM] = {440.0f, 880.0f, 1200.0f, 1600.0f}; // 四個通道不同的頻率
  const int frame_count = AUDIO_IN_PACKET / (HALF_WORD_BYTES * IN_CHANNEL_NUM);
  const int packet_size = AUDIO_IN_PACKET;

  while (1)
  {
    if (!usb_device_is_configured(busid) || !tx_flag)
    {
      vTaskDelay(1);
      continue;
    }

    memset(write_buffer, 0, 4);
    int16_t *pcm_buf = (int16_t *)(write_buffer);
    int idx = 0;
    for (int frame = 0; frame < frame_count; frame++)
    {
      for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
      {
        float val = sinf(g_phase[ch]);
        pcm_buf[idx++] = (int16_t)(val * 32767.0f);

        g_phase[ch] += 2.0f * M_PI * freqs[ch] / (float)s_mic_sample_rate;
        if (g_phase[ch] >= 2.0f * M_PI)
        {
          g_phase[ch] -= 2.0f * M_PI;
        }
      }
    }
    ESP_LOGI("USB_AUDIO_DEMO", "Generated %d bytes of audio data", packet_size);
    usbd_ep_start_write(busid, AUDIO_IN_EP, write_buffer, packet_size);

    uint32_t feedback_value = AUDIO_FREQ_TO_FEEDBACK_FS(s_mic_sample_rate);
    AUDIO_FEEDBACK_TO_BUF_FS(feedback_buffer, feedback_value);
    usbd_ep_start_write(busid, AUDIO_IN_FEEDBACK_EP, feedback_buffer, AUDIO_IN_FEEDBACK_PACKET_SIZE);

    if (ep_tx_done_sem != NULL)
    {
      if (xSemaphoreTake(ep_tx_done_sem, pdMS_TO_TICKS(1000)) != pdTRUE)
      {
        ESP_LOGW("USB_AUDIO_DEMO", "USB TX timeout waiting for callback");
      }
    }
    ESP_LOGI("USB_AUDIO_DEMO", "Audio data sent");
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