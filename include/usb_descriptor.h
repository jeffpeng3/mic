#ifndef USB_DESCRIPTOR_H
#define USB_DESCRIPTOR_H

#include <stdint.h>
#include "dwc2_config.h"
#include "usbd_core.h"
#include "usbd_audio.h"
#define USBD_VID 0x045E
#define USBD_PID 0x1234
#define USBD_MAX_POWER 100
#define USBD_LANGID_STRING 0x0409

#ifdef CONFIG_USB_HS
#define EP_INTERVAL 0x04
#else
#define EP_INTERVAL 0x01
#endif

#define AUDIO_IN_EP 0x81

#define AUDIO_IN_FU_ID 0x02

#define HALF_WORD_BYTES 2 // 2 half word (one channel)
#define SAMPLE_BITS 16    // 16 bit per channel

#define AUDIO_IN_MAX_FREQ 48000
#define IN_CHANNEL_NUM 4

#if IN_CHANNEL_NUM == 1
#define INPUT_CTRL 0x03, 0x03
#define INPUT_CH_ENABLE 0x0000
#elif IN_CHANNEL_NUM == 2
#define INPUT_CTRL 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x0003
#elif IN_CHANNEL_NUM == 3
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x0007
#elif IN_CHANNEL_NUM == 4
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x000f
#elif IN_CHANNEL_NUM == 5
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x001f
#elif IN_CHANNEL_NUM == 6
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x003F
#elif IN_CHANNEL_NUM == 7
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x007f
#elif IN_CHANNEL_NUM == 8
#define INPUT_CTRL 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
#define INPUT_CH_ENABLE 0x00ff
#endif

#define AUDIO_IN_PACKET ((uint32_t)((AUDIO_IN_MAX_FREQ * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000))

#define USB_AUDIO_CONFIG_DESC_SIZ (9 +                                                    \
                                   AUDIO_AC_DESCRIPTOR_LEN(1) +                           \
                                   AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                 \
                                   AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(IN_CHANNEL_NUM, 1) + \
                                   AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC +                \
                                   AUDIO_AS_DESCRIPTOR_LEN(1))

#define AUDIO_AC_SIZ (AUDIO_SIZEOF_AC_HEADER_DESC(1) +                       \
                      AUDIO_SIZEOF_AC_INPUT_TERMINAL_DESC +                  \
                      AUDIO_SIZEOF_AC_FEATURE_UNIT_DESC(IN_CHANNEL_NUM, 1) + \
                      AUDIO_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

extern const struct usb_descriptor audio_v1_descriptor;


extern const uint8_t mic_default_sampling_freq_table[];

#endif /* USB_DESCRIPTOR_H */