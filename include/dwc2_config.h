#include "esp_log.h"
#include "port/dwc2/usb_dwc2_param.h"

void dwc2_get_user_fifo_config(uint32_t reg_base, struct usb_dwc2_user_fifo_config *fifo_config)
{
    ESP_LOGD("USB", "Getting user FIFO config");
    fifo_config->device_tx_fifo_size[0] = 16; // 64 bytes for EP0 IN
    fifo_config->device_tx_fifo_size[1] = 100; // 388 bytes for EP1 IN (384-byte audio packet + status)
    fifo_config->device_tx_fifo_size[2] = 4;  // 16 bytes for EP2 IN feedback
    fifo_config->device_tx_fifo_size[3] = 0;
    fifo_config->device_tx_fifo_size[4] = 0;
    fifo_config->device_tx_fifo_size[5] = 0;
    fifo_config->device_tx_fifo_size[6] = 0;
    int reserved = 128;
    for (int i = 0; i < MAX_EPS_CHANNELS; i++)
    {
        ESP_LOGD("USB", "User FIFO config - ep%d tx fifo size: %d", i, fifo_config->device_tx_fifo_size[i]);
        reserved -= fifo_config->device_tx_fifo_size[i];
        if (fifo_config->device_tx_fifo_size[i] <= 0)
        {
            break;
        }
    }
    ESP_LOGD("USB", "User FIFO config reserved words: %d", reserved);
    fifo_config->device_rx_fifo_size = 47;
}