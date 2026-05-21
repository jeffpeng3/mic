#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include "custom_usb_audio.h"

#define TAG "USB_AUDIO"

#define AUDIO_SILENCE_BUFFER_COUNT 1
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t audio_transfer_buffers[AUDIO_TRANSFER_BUFFER_COUNT][AUDIO_IN_PACKET];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t audio_silence_buffer[AUDIO_IN_PACKET];

static volatile bool tx_flag = false;
static QueueHandle_t audio_ready_queue = NULL;
static QueueHandle_t audio_free_queue = NULL;
static volatile uint32_t s_mic_sample_rate = AUDIO_IN_MAX_FREQ;
static volatile int current_mic_volume = 100;
static uint8_t *previous_tx_buffer = NULL;
static bool previous_tx_buffer_is_pool = false;

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

uint32_t usb_audio_get_packet_size(void)
{
    uint32_t packet_size = (s_mic_sample_rate * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
    if (packet_size > AUDIO_IN_PACKET)
    {
        packet_size = AUDIO_IN_PACKET;
    }
    return packet_size;
}

void usbd_audio_open(uint8_t busid, uint8_t intf)
{
    tx_flag = true;
    USB_LOG_RAW("OPEN\r\n");

    uint32_t packet_size = usb_audio_get_packet_size();
    memset(audio_silence_buffer, 0, packet_size);
    previous_tx_buffer = NULL;
    previous_tx_buffer_is_pool = false;
    usbd_ep_start_write(busid, AUDIO_IN_EP, audio_silence_buffer, packet_size);
}

void usbd_audio_close(uint8_t busid, uint8_t intf)
{
    USB_LOG_RAW("CLOSE\r\n");
    tx_flag = false;
}

void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume)
{
    (void)busid;
    (void)ep;
    (void)ch;
    current_mic_volume = volume;
}

int usbd_audio_get_volume(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;
    return current_mic_volume;
}

void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute)
{
    (void)busid;
    (void)ep;
    (void)ch;
    (void)mute;
}

bool usbd_audio_get_mute(uint8_t busid, uint8_t ep, uint8_t ch)
{
    (void)busid;
    (void)ep;
    (void)ch;
    return false;
}

void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq)
{
    (void)busid;
    if (ep == AUDIO_IN_EP)
    {
        s_mic_sample_rate = sampling_freq;
    }
}

uint32_t usbd_audio_get_sampling_freq(uint8_t busid, uint8_t ep)
{
    (void)busid;
    if (ep == AUDIO_IN_EP)
    {
        return s_mic_sample_rate;
    }
    return 0;
}

void usbd_audio_get_sampling_freq_table(uint8_t busid, uint8_t ep, uint8_t **sampling_freq_table)
{
    (void)busid;
    if (ep == AUDIO_IN_EP)
    {
        *sampling_freq_table = (uint8_t *)mic_default_sampling_freq_table;
    }
}

void usbd_audio_iso_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    (void)nbytes;
    if (!tx_flag)
    {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t packet_size = usb_audio_get_packet_size();
    uint8_t *next_buffer = NULL;

    if (audio_ready_queue != NULL && xQueueReceiveFromISR(audio_ready_queue, &next_buffer, &xHigherPriorityTaskWoken) == pdTRUE)
    {
        if (previous_tx_buffer != NULL && previous_tx_buffer_is_pool && audio_free_queue != NULL)
        {
            xQueueSendFromISR(audio_free_queue, &previous_tx_buffer, &xHigherPriorityTaskWoken);
        }
        previous_tx_buffer = next_buffer;
        previous_tx_buffer_is_pool = true;
        usbd_ep_start_write(busid, AUDIO_IN_EP, next_buffer, packet_size);
    }
    else
    {
        if (previous_tx_buffer != NULL && previous_tx_buffer_is_pool && audio_free_queue != NULL)
        {
            xQueueSendFromISR(audio_free_queue, &previous_tx_buffer, &xHigherPriorityTaskWoken);
        }
        previous_tx_buffer = NULL;
        previous_tx_buffer_is_pool = false;
        memset(audio_silence_buffer, 0, packet_size);
        usbd_ep_start_write(busid, AUDIO_IN_EP, audio_silence_buffer, packet_size);
    }

    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static struct usbd_endpoint audio_in_ep = {
    .ep_cb = usbd_audio_iso_in_callback,
    .ep_addr = AUDIO_IN_EP,
};

static struct usbd_interface intf0;
static struct usbd_interface intf1;

static struct audio_entity_info audio_entity_table[] = {
    {.bEntityId = AUDIO_IN_FU_ID,
     .bDescriptorSubtype = AUDIO_CONTROL_FEATURE_UNIT,
     .ep = AUDIO_IN_EP},
};

static esp_err_t audio_v2_init(void)
{
    const uint8_t busid = 0;
    const uintptr_t reg_base = ESP_USBD_BASE;

    usbd_desc_register(busid, &audio_v1_descriptor);
    usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf0, 0x0100, audio_entity_table, 1));
    usbd_add_interface(busid, usbd_audio_init_intf(busid, &intf1, 0x0100, audio_entity_table, 1));
    usbd_add_endpoint(busid, &audio_in_ep);

    int ret = usbd_initialize(busid, reg_base, usbd_event_handler);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "usbd_initialize failed: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t usb_audio_init(QueueHandle_t ready_queue, QueueHandle_t free_queue)
{
    if (ready_queue == NULL || free_queue == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    audio_ready_queue = ready_queue;
    audio_free_queue = free_queue;

    for (int i = 0; i < AUDIO_TRANSFER_BUFFER_COUNT; i++)
    {
        uint8_t *buffer = audio_transfer_buffers[i];
        xQueueSend(audio_free_queue, &buffer, 0);
    }

    memset(audio_silence_buffer, 0, AUDIO_IN_PACKET);
    return audio_v2_init();
}
