#include "esp_log.h"
#include "port/dwc2/usb_dwc2_param.h"

void dwc2_get_user_fifo_config(uint32_t reg_base, struct usb_dwc2_user_fifo_config *fifo_config)
{
    ESP_LOGI("USB", "Getting user FIFO config");
    fifo_config->device_tx_fifo_size[0] = 16; // 64 byte
    fifo_config->device_tx_fifo_size[1] = 100; // 256 byte
    fifo_config->device_tx_fifo_size[2] = 16; // 64 byte
    fifo_config->device_tx_fifo_size[3] = 0;  // 64 byte
    fifo_config->device_tx_fifo_size[4] = 0;  // 0 byte
    fifo_config->device_tx_fifo_size[5] = 0;  // 0 byte
    fifo_config->device_tx_fifo_size[6] = 0;  // 0 byte
    int reserved = 128;
    for (int i = 0; i < MAX_EPS_CHANNELS; i++)
    {
        ESP_LOGI("USB", "User FIFO config - ep%d tx fifo size: %d", i, fifo_config->device_tx_fifo_size[i]);
        reserved -= fifo_config->device_tx_fifo_size[i];
        if (fifo_config->device_tx_fifo_size[i] <= 0)
        {
            break;
        }
    }
    fifo_config->device_rx_fifo_size = 47;
}