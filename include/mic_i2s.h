#ifndef MIC_I2S_H
#define MIC_I2S_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mic_i2s_init(void);
esp_err_t mic_i2s_read(void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms);
void mic_i2s_start_task(StreamBufferHandle_t stream);

#ifdef __cplusplus
}
#endif

#endif // MIC_I2S_H
