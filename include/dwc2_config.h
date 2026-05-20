#ifndef DWC2_CONFIG_H
#define DWC2_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "port/dwc2/usb_dwc2_param.h"

void dwc2_get_user_fifo_config(uint32_t reg_base, struct usb_dwc2_user_fifo_config *fifo_config);

#endif // DWC2_CONFIG_H
