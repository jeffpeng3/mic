#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include <mic_sim.h>

#define TAG "MIC_SIM"

struct mic_sim_task_param {
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
    uint32_t sampling_freq_table[4];
};
uint16_t sin_table[16384];

void mic_sim_task(void *param)
{
    struct mic_sim_task_param *task_param = (struct mic_sim_task_param *)param;
    QueueHandle_t free_queue = task_param->free_queue;
    QueueHandle_t ready_queue = task_param->ready_queue;
    uint32_t sampling_freq_table[4];
    for (int i = 0; i < 4; i++)
    {
        sampling_freq_table[i] = task_param->sampling_freq_table[i];
    }
    free(task_param);
    uint32_t state[IN_CHANNEL_NUM] = {0};
    uint32_t phase_increment[IN_CHANNEL_NUM];
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
        uint8_t *output_buffer = NULL;
        if (xQueueReceive(free_queue, &output_buffer, portMAX_DELAY) != pdTRUE || output_buffer == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // generate PCM data for one packet of 4 channels 48kHz 16bit by simulating a sine wave with frequency from sampling_freq_table
        int16_t *pcm_out = (int16_t *)output_buffer;
        for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
        {
            for (int j = 0; j < AUDIO_IN_PACKET / sizeof(int16_t) / IN_CHANNEL_NUM; j++)
            {
                pcm_out[j * IN_CHANNEL_NUM + ch] = (int16_t)sin_table[(state[ch] >> 18) % 16384];
                state[ch] += phase_increment[ch];
            }
        }

        xQueueSend(ready_queue, &output_buffer, portMAX_DELAY);
    }
}

void mic_sim_start_task(QueueHandle_t free_queue, QueueHandle_t ready_queue, const uint32_t *sampling_freq_table)
{
    struct mic_sim_task_param *task_param = malloc(sizeof(struct mic_sim_task_param));
    if (task_param == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate mic sim task parameters");
        return;
    }
    task_param->free_queue = free_queue;
    task_param->ready_queue = ready_queue;
    for (int i = 0; i < 4; i++)
    {
        task_param->sampling_freq_table[i] = sampling_freq_table[i];
    }
    ESP_LOGI(TAG, "Starting mic sim task with sampling frequencies: %d, %d, %d, %d",
             sampling_freq_table[0], sampling_freq_table[1], sampling_freq_table[2], sampling_freq_table[3]);
    xTaskCreatePinnedToCore(mic_sim_task, "mic_sim", 4096, (void *)task_param, 5, NULL, tskNO_AFFINITY);
}
