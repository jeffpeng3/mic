#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include <mic_sim.h>

#define TAG "MIC_SIM"

struct mic_sim_task_param {
    StreamBufferHandle_t stream;
    uint32_t sampling_freq_table[4];
};
uint16_t sin_table[16384];

void mic_sim_task(void *param)
{
    struct mic_sim_task_param *task_param = (struct mic_sim_task_param *)param;
    StreamBufferHandle_t write_stream = task_param->stream;
    uint32_t sampling_freq_table[4];
    for (int i = 0; i < 4; i++)
    {
        sampling_freq_table[i] = task_param->sampling_freq_table[i];
    }
    uint32_t state[IN_CHANNEL_NUM] = {0};
    uint32_t phase_increment[IN_CHANNEL_NUM];
    int16_t pcm_data[AUDIO_IN_PACKET / sizeof(int16_t)];
    for (int j = 0; j < 16384; j++)
    {
        sin_table[j] = (uint16_t)(32767 * sin(2 * M_PI * j / 16384));
    }
    for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
    {
        phase_increment[ch] = (uint32_t)(((uint64_t)sampling_freq_table[ch] * 16384ULL << 18) / 48000ULL);
    }
    while (true)
    {
        // generate PCM data for one packet of 4 channels 48kHz 16bit by simulating a sine wave with frequency from sampling_freq_table
        for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
        {
            for (int j = 0; j < AUDIO_IN_PACKET / sizeof(int16_t) / IN_CHANNEL_NUM; j++)
            {
                pcm_data[j * IN_CHANNEL_NUM + ch] = (int16_t)sin_table[(state[ch] >> 18) % 16384];
                state[ch] += phase_increment[ch];
            }
        }
        if (write_stream != NULL)
        {
            xStreamBufferSend(write_stream, pcm_data, sizeof(pcm_data), portMAX_DELAY);
        }
    }
}

void mic_sim_start_task(StreamBufferHandle_t stream, const uint32_t *sampling_freq_table)
{
    struct mic_sim_task_param *task_param = malloc(sizeof(struct mic_sim_task_param));
    task_param->stream = stream;
    for (int i = 0; i < 4; i++)
    {
        task_param->sampling_freq_table[i] = sampling_freq_table[i];
    }
    ESP_LOGI(TAG, "Starting mic sim task with sampling frequencies: %d, %d, %d, %d",
             sampling_freq_table[0], sampling_freq_table[1], sampling_freq_table[2], sampling_freq_table[3]);
    xTaskCreatePinnedToCore(mic_sim_task, "mic_sim", 4096, (void *)task_param, 5, NULL, tskNO_AFFINITY);
}
