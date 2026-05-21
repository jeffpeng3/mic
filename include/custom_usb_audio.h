#ifndef CUSTOM_USB_AUDIO_H
#define CUSTOM_USB_AUDIO_H

#include <esp_err.h>
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Increase USB audio packet buffering to reduce dropouts under scheduler jitter. */
#define AUDIO_TRANSFER_BUFFER_COUNT 16

esp_err_t usb_audio_init(QueueHandle_t ready_queue, QueueHandle_t free_queue);
uint32_t usb_audio_get_packet_size(void);

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_USB_AUDIO_H
