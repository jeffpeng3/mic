#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb_descriptor.h"
#include <mic_i2s.h>

#define TAG "MIC_I2S"
#define I2S_SAMPLE_RATE 48000

static i2s_chan_handle_t rx_chan = NULL;

esp_err_t mic_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 128;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_chan);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 36,
            .ws = 35,
            .dout = I2S_GPIO_UNUSED,
            .din = 37,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(rx_chan, &std_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(rx_chan);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "INMP441 I2S init done: SD=37, SCK=36, WS=35, SR=%u", I2S_SAMPLE_RATE);
    return ESP_OK;
}

esp_err_t mic_i2s_read(void *dest, size_t size, size_t *bytes_read, uint32_t timeout_ms)
{
    if (rx_chan == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return i2s_channel_read(rx_chan, dest, size, bytes_read, timeout_ms);
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

static void mic_capture_task(void *arg)
{
    StreamBufferHandle_t write_stream = (StreamBufferHandle_t)arg;
    int16_t pcm_local_buf[AUDIO_IN_PACKET / HALF_WORD_BYTES];
    int32_t i2s_read_buf[AUDIO_IN_PACKET / sizeof(int32_t)];

    while (1)
    {
        uint32_t packet_size = mic_get_packet_size();
        if (packet_size == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const int frame_count_per_packet = packet_size / (HALF_WORD_BYTES * IN_CHANNEL_NUM);
        size_t read_size = frame_count_per_packet * 2 * sizeof(int32_t);
        if (read_size > sizeof(i2s_read_buf))
        {
            read_size = sizeof(i2s_read_buf);
        }

        size_t bytes_read = 0;
        esp_err_t err = mic_i2s_read(i2s_read_buf, read_size, &bytes_read, portMAX_DELAY);
        if (err != ESP_OK || bytes_read == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int samples_read = bytes_read / sizeof(int32_t);
        int frames_to_fill = frame_count_per_packet;
        int available_frames = samples_read / 2;
        if (available_frames < frames_to_fill)
        {
            frames_to_fill = available_frames;
        }

        for (int frame = 0; frame < frames_to_fill; frame++)
        {
            int32_t sample_r = i2s_read_buf[frame * 2];
            int32_t sample_l = i2s_read_buf[frame * 2 + 1];
            uint32_t abs_r = sample_r == INT32_MIN ? INT32_MAX : (uint32_t)(sample_r < 0 ? -sample_r : sample_r);
            uint32_t abs_l = sample_l == INT32_MIN ? INT32_MAX : (uint32_t)(sample_l < 0 ? -sample_l : sample_l);
            int32_t sample32 = abs_r >= abs_l ? sample_r : sample_l;
            int16_t sample16 = (int16_t)(sample32 >> 16);

            int idx = frame * IN_CHANNEL_NUM;
            for (int ch = 0; ch < IN_CHANNEL_NUM; ch++)
            {
                pcm_local_buf[idx++] = sample16;
            }
        }

        if (frames_to_fill < frame_count_per_packet)
        {
            int start = frames_to_fill * IN_CHANNEL_NUM;
            int count = (frame_count_per_packet - frames_to_fill) * IN_CHANNEL_NUM;
            memset(&pcm_local_buf[start], 0, count * sizeof(int16_t));
        }

        if (write_stream != NULL)
        {
            xStreamBufferSend(write_stream, pcm_local_buf, packet_size, portMAX_DELAY);
        }
    }
}

void mic_i2s_start_task(StreamBufferHandle_t stream)
{
    xTaskCreatePinnedToCore(mic_capture_task, "mic_capture", 4096, (void *)stream, 5, NULL, tskNO_AFFINITY);
}
