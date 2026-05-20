#ifndef MIC_SIM_H
#define MIC_SIM_H

#include <esp_err.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void mic_sim_start_task(StreamBufferHandle_t stream, const uint32_t *sampling_freq_table);

#ifdef __cplusplus
}
#endif

#endif // MIC_SIM_H
