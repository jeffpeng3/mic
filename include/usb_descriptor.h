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

#define EP_INTERVAL 0x01

#define AUDIO_IN_EP 0x81
#define AUDIO_IN_FEEDBACK_EP 0x82
#define AUDIO_IN_FEEDBACK_PACKET_SIZE 3

#define AUDIO_IN_CLOCK_ID 0x01
#define AUDIO_IN_FU_ID 0x03

#define AUDIO_IN_MAX_FREQ 48000
#define HALF_WORD_BYTES 2 // 2 half word (one channel)
#define SAMPLE_BITS 16    // 16 bit per channel

#define BMCONTROL (AUDIO_V2_FU_CONTROL_MUTE | AUDIO_V2_FU_CONTROL_VOLUME)

#define IN_CHANNEL_NUM 4

// #define INPUT_CTRL DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
// #define INPUT_CH_ENABLE 0x00000003
#define INPUT_CTRL DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL), DBVAL(BMCONTROL)
#define INPUT_CH_ENABLE 0x0000000f

#define AUDIO_IN_PACKET ((uint32_t)((AUDIO_IN_MAX_FREQ * HALF_WORD_BYTES * IN_CHANNEL_NUM) / 1000))

#define USB_AUDIO_CONFIG_DESC_SIZ (9 +                                                    \
                                   AUDIO_V2_AC_DESCRIPTOR_INIT_LEN +                      \
                                   AUDIO_V2_SIZEOF_AC_CLOCK_SOURCE_DESC +                 \
                                   AUDIO_V2_SIZEOF_AC_INPUT_TERMINAL_DESC +               \
                                   AUDIO_V2_SIZEOF_AC_FEATURE_UNIT_DESC(IN_CHANNEL_NUM) + \
                                   AUDIO_V2_SIZEOF_AC_OUTPUT_TERMINAL_DESC +              \
                                   AUDIO_V2_AS_FEEDBACK_DESCRIPTOR_INIT_LEN)

#define AUDIO_AC_SIZ (AUDIO_V2_SIZEOF_AC_HEADER_DESC +                       \
                      AUDIO_V2_SIZEOF_AC_CLOCK_SOURCE_DESC +                 \
                      AUDIO_V2_SIZEOF_AC_INPUT_TERMINAL_DESC +               \
                      AUDIO_V2_SIZEOF_AC_FEATURE_UNIT_DESC(IN_CHANNEL_NUM) + \
                      AUDIO_V2_SIZEOF_AC_OUTPUT_TERMINAL_DESC)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0001, 0x01)};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_AUDIO_CONFIG_DESC_SIZ, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    AUDIO_V2_AC_DESCRIPTOR_INIT(0x00, 0x02, AUDIO_AC_SIZ, AUDIO_CATEGORY_MICROPHONE, 0x00, 0x00),
    AUDIO_V2_AC_CLOCK_SOURCE_DESCRIPTOR_INIT(0x01, 0x03, 0x03),
    AUDIO_V2_AC_INPUT_TERMINAL_DESCRIPTOR_INIT(0x02, AUDIO_INTERM_MIC, 0x01, IN_CHANNEL_NUM, INPUT_CH_ENABLE, 0x0000),
    AUDIO_V2_AC_FEATURE_UNIT_DESCRIPTOR_INIT(0x03, 0x02, INPUT_CTRL),
    AUDIO_V2_AC_OUTPUT_TERMINAL_DESCRIPTOR_INIT(0x04, AUDIO_TERMINAL_STREAMING, 0x03, 0x01, 0x0000),
    AUDIO_V2_AS_FEEDBACK_DESCRIPTOR_INIT(0x01, 0x04, IN_CHANNEL_NUM, INPUT_CH_ENABLE, HALF_WORD_BYTES, SAMPLE_BITS, AUDIO_IN_EP, (AUDIO_IN_PACKET), EP_INTERVAL, AUDIO_IN_FEEDBACK_EP)};

static const uint8_t device_quality_descriptor[] = {
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x00,
    0x00,
};

static const char *string_descriptors[] = {
    (const char[]){0x09, 0x04}, /* Langid */
    "CheraaaryUSB",             /* Manufacturer */
    "Che DEMO",                 /* Product */
    "20221z23456",              /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index > 3)
    {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor audio_v2_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback};

static const uint8_t mic_default_sampling_freq_table[] = {
    AUDIO_SAMPLE_FREQ_NUM(1),
    AUDIO_SAMPLE_FREQ_4B(48000),
    AUDIO_SAMPLE_FREQ_4B(48000),
    AUDIO_SAMPLE_FREQ_4B(0x00)};

#endif /* USB_DESCRIPTOR_H */