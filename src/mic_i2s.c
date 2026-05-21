#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include <mic_i2s.h>

#define TAG "MIC_I2S"
#define I2S_SAMPLE_RATE 48000

#define I2S1_WS_PIN 35
#define I2S1_BCLK_PIN 36
#define I2S1_DIN_PIN 37

#define I2S2_WS_PIN 5
#define I2S2_BCLK_PIN 6
#define I2S2_DIN_PIN 7

static i2s_chan_handle_t rx_chan1 = NULL;
static i2s_chan_handle_t rx_chan2 = NULL;

static void mic_i2s_discard_initial_samples(i2s_chan_handle_t channel)
{
    int32_t discard_buf[AUDIO_IN_PACKET / sizeof(int32_t)];
    size_t bytes_read = 0;
    for (int i = 0; i < 2; i++)
    {
        i2s_channel_read(channel, discard_buf, sizeof(discard_buf), &bytes_read, pdMS_TO_TICKS(100));
        (void)bytes_read;
    }
}

esp_err_t mic_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 128;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_chan1);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_new_channel(1) failed: %s", esp_err_to_name(err));
        return err;
    }

    chan_cfg.id = I2S_NUM_1;
    err = i2s_new_channel(&chan_cfg, NULL, &rx_chan2);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_new_channel(2) failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg1 = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S1_BCLK_PIN,
            .ws = I2S1_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S1_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(rx_chan1, &std_cfg1);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode(1) failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg2 = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S2_BCLK_PIN,
            .ws = I2S2_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S2_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(rx_chan2, &std_cfg2);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode(2) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(rx_chan1);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_enable(1) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(rx_chan2);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_enable(2) failed: %s", esp_err_to_name(err));
        return err;
    }

    mic_i2s_discard_initial_samples(rx_chan1);
    mic_i2s_discard_initial_samples(rx_chan2);

    ESP_LOGI(TAG, "I2S stereo init done: CH1(SD=%d,SCK=%d,WS=%d) CH2(SD=%d,SCK=%d,WS=%d), SR=%u",
             I2S1_DIN_PIN, I2S1_BCLK_PIN, I2S1_WS_PIN,
             I2S2_DIN_PIN, I2S2_BCLK_PIN, I2S2_WS_PIN,
             I2S_SAMPLE_RATE);
    return ESP_OK;
}

static esp_err_t mic_i2s_read_channel(i2s_chan_handle_t channel, void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    if (channel == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(channel, dest, size, bytes_read, timeout_ms);
}


static uint32_t mic_get_packet_size(void)
{
    uint32_t packet_size = (I2S_SAMPLE_RATE * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000;
    if (packet_size > AUDIO_IN_PACKET)
    {
        packet_size = AUDIO_IN_PACKET;
    }
    return packet_size;
}

struct mic_i2s_task_param {
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
};

static void mic_capture_task(void *arg)
{
    struct mic_i2s_task_param *task_param = (struct mic_i2s_task_param *)arg;
    QueueHandle_t free_queue = task_param->free_queue;
    QueueHandle_t ready_queue = task_param->ready_queue;
    free(task_param);

    int32_t i2s_read_buf[AUDIO_IN_PACKET / sizeof(int32_t)];
    int32_t i2s_read_buf2[AUDIO_IN_PACKET / sizeof(int32_t)];

    while (1)
    {
        uint8_t *output_buffer = NULL;
        if (xQueueReceive(free_queue, &output_buffer, portMAX_DELAY) != pdTRUE || output_buffer == NULL)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        uint32_t packet_size = mic_get_packet_size();
        if (packet_size == 0)
        {
            xQueueSend(free_queue, &output_buffer, portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const int frame_count_per_packet = packet_size / (HALF_WORD_BYTES * IN_CHANNEL_NUM);
        size_t read_size = frame_count_per_packet * 2 * sizeof(int32_t);
        if (read_size > sizeof(i2s_read_buf))
        {
            read_size = sizeof(i2s_read_buf);
        }

        size_t bytes_read1 = 0;
        esp_err_t err1 = mic_i2s_read_channel(rx_chan1, i2s_read_buf, read_size, &bytes_read1, portMAX_DELAY);
        size_t bytes_read2 = 0;
        esp_err_t err2 = mic_i2s_read_channel(rx_chan2, i2s_read_buf2, read_size, &bytes_read2, portMAX_DELAY);
        if (err1 != ESP_OK || err2 != ESP_OK || bytes_read1 == 0 || bytes_read2 == 0)
        {
            xQueueSend(free_queue, &output_buffer, portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int samples_read1 = bytes_read1 / sizeof(int32_t);
        int samples_read2 = bytes_read2 / sizeof(int32_t);
        int frames_to_fill = frame_count_per_packet;
        int available_frames1 = samples_read1 / 2;
        int available_frames2 = samples_read2 / 2;
        if (available_frames1 < frames_to_fill)
        {
            frames_to_fill = available_frames1;
        }
        if (available_frames2 < frames_to_fill)
        {
            frames_to_fill = available_frames2;
        }

        int16_t *pcm_out = (int16_t *)output_buffer;
        for (int frame = 0; frame < frames_to_fill; frame++)
        {
            int32_t sample1_l = i2s_read_buf[frame * 2];
            int32_t sample1_r = i2s_read_buf[frame * 2 + 1];
            int32_t sample2_l = i2s_read_buf2[frame * 2];
            int32_t sample2_r = i2s_read_buf2[frame * 2 + 1];
            int16_t sample16_1_l = (int16_t)(sample1_l >> 13);
            int16_t sample16_1_r = (int16_t)(sample1_r >> 13);
            int16_t sample16_2_l = (int16_t)(sample2_l >> 13);
            int16_t sample16_2_r = (int16_t)(sample2_r >> 13);

            int idx = frame * IN_CHANNEL_NUM;
            pcm_out[idx++] = sample16_1_r;
            pcm_out[idx++] = sample16_1_l;
            pcm_out[idx++] = sample16_2_r;
            pcm_out[idx++] = sample16_2_l;
        }

        if (frames_to_fill < frame_count_per_packet)
        {
            int start = frames_to_fill * IN_CHANNEL_NUM;
            int count = (frame_count_per_packet - frames_to_fill) * IN_CHANNEL_NUM;
            memset(&pcm_out[start], 0, count * sizeof(int16_t));
        }

        xQueueSend(ready_queue, &output_buffer, portMAX_DELAY);
    }
}

void mic_i2s_start_task(QueueHandle_t free_queue, QueueHandle_t ready_queue)
{
    struct mic_i2s_task_param *task_param = malloc(sizeof(struct mic_i2s_task_param));
    if (task_param == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate I2S task parameters");
        return;
    }
    task_param->free_queue = free_queue;
    task_param->ready_queue = ready_queue;
    xTaskCreatePinnedToCore(mic_capture_task, "mic_capture", 4096, task_param, 5, NULL, tskNO_AFFINITY);
}
