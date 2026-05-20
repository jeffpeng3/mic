#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include "custom_usb_audio.h"

#define TAG "USB_AUDIO"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[AUDIO_IN_PACKET];

static volatile bool tx_flag = false;
static StreamBufferHandle_t audio_read_stream = NULL;
static volatile uint32_t s_mic_sample_rate = AUDIO_IN_MAX_FREQ;
static volatile int current_mic_volume = 100;

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
    memset(write_buffer, 0, packet_size);
    usbd_ep_start_write(busid, AUDIO_IN_EP, write_buffer, packet_size);
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

    size_t received = 0;
    if (audio_read_stream != NULL)
    {
        received = xStreamBufferReceiveFromISR(audio_read_stream, write_buffer, packet_size, &xHigherPriorityTaskWoken);
    }

    if (received < packet_size)
    {
        memset(write_buffer + received, 0, packet_size - received);
    }

    usbd_ep_start_write(busid, AUDIO_IN_EP, write_buffer, packet_size);

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

esp_err_t usb_audio_init(StreamBufferHandle_t stream)
{
    audio_read_stream = stream;
    return audio_v2_init();
}
