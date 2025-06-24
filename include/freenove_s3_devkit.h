#ifndef FREENOVE_S3_DEVKIT_H
#define FREENOVE_S3_DEVKIT_H
#include <stdint.h>
#include <stddef.h>
#include "esp_attr.h"
#include "driver/sdmmc_types.h"
enum {
    CAM_DEFAULT = 0,
    CAM_ALLOC_FB_PSRAM=(1<<0),
    CAM_ALLOC_CAM_PSRAM=(1<<1),
    CAM_FRAME_SIZE_96X96=(1<<2)
};
typedef enum {
    CAM_NO_CHANGE = -3,
    CAM_LOWEST,
    CAM_LOW,
    CAM_MEDIUM,
    CAM_HIGH,
    CAM_HIGHEST
} cam_level_t;

enum {
    TOUCH_THRESH_DEFAULT = 32
};

typedef enum {
    AUDIO_44_1K_STEREO=0,
    AUDIO_44_1K_MONO,
    AUDIO_22K_STEREO,
    AUDIO_22K_MONO,
    AUDIO_11K_STEREO,
    AUDIO_11K_MONO,
} audio_format_t;

enum {
    SD_FLAGS_DEFAULT = 0,
    SD_FLAGS_FORMAT_ON_FAIL = 1
};
enum {
    SD_MAX_FILES_DEFAULT = 5
};
enum {
    SD_ALLOC_SIZE_DEFAULT = 0
};
enum {
    SD_FREQ_DEFAULT = 20*1000
};

#define SD_MOUNT_POINT_DEFAULT "/sdcard"

#define AUDIO_MAX_SAMPLES 1024

#ifdef __cplusplus
extern "C" {
#endif

// optionally implemented by user: notify when transfer complete
extern IRAM_ATTR void lcd_on_flush_complete(void) __attribute__((weak));
extern void lcd_initialize(size_t max_transfer_size);
extern void lcd_rotation(int rotation);
extern void lcd_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const void* bitmap);
extern int lcd_wait_flush(uint32_t timeout);
extern void lcd_deinitialize(void);

extern void led_initialize(void);
extern void led_enable(int enabled);
extern void led_deinitialize(void);

extern void camera_initialize(int flags);
extern void camera_levels(int brightness, int contrast,
                   int saturation, int sharpness);
extern void camera_rotation(int rotation);
extern const void* camera_frame_buffer(void);
extern void camera_deinitialize(void);

extern void neopixel_initialize(void);
extern void neopixel_color(uint8_t r, uint8_t g, uint8_t b);
extern void neopixel_deinitialize(void);

extern void touch_initialize(int threshhold);
extern void touch_rotation(int rotation);
extern int touch_xy(uint16_t* out_x, uint16_t* out_y);
extern int touch_xy2(uint16_t* out_x, uint16_t* out_y);
extern void touch_deinitialize(void);

extern void audio_initialize(audio_format_t format);
extern void audio_deinitialize(void);
extern size_t audio_write_int16(const int16_t* samples, size_t sample_count);
extern size_t audio_write_float(const float* samples, size_t sample_count, float vel);

extern int sd_initialize(const char* mount_point, size_t max_files, size_t allocation_unit_size, uint32_t freq_khz, int flags);
extern void sd_deinitialize();
extern sdmmc_card_t* sd_card();

#ifdef __cplusplus
}
#endif
#endif // FREENOVE_S3_DEVKIT_H