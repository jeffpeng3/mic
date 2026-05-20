#ifndef CUSTOM_USB_AUDIO_H
#define CUSTOM_USB_AUDIO_H

#include <esp_err.h>
#include "freertos/stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t usb_audio_init(StreamBufferHandle_t stream);
uint32_t usb_audio_get_packet_size(void);

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_USB_AUDIO_H
