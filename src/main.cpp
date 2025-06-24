#include <stdlib.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "freenove_s3_devkit.h"
#define JETBRAINSMONO_IMPLEMENTATION
#include "assets/jetBrainsMono.hpp"
#include <gfx.hpp>
#include <uix.hpp>

using namespace gfx;
using namespace uix;

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

const char* hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "htcw",             // 1: Manufacturer
    "masher",      // 2: Product
    "0100110",              // 3: Serials, replace with chip ID
    "Masher HID interface",  // 4: HID
};

static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}


typedef enum {
    MOUSE_DIR_RIGHT,
    MOUSE_DIR_DOWN,
    MOUSE_DIR_LEFT,
    MOUSE_DIR_UP,
    MOUSE_DIR_MAX,
} mouse_dir_t;

#define DISTANCE_MAX        125
#define DELTA_SCALAR        5

static void mouse_draw_square_next_delta(int8_t *delta_x_ret, int8_t *delta_y_ret)
{
    static int cur_dir = (int)MOUSE_DIR_RIGHT;
    static uint32_t distance = 0;

    // Calculate next delta
    if (cur_dir == MOUSE_DIR_RIGHT) {
        *delta_x_ret = DELTA_SCALAR;
        *delta_y_ret = 0;
    } else if (cur_dir == MOUSE_DIR_DOWN) {
        *delta_x_ret = 0;
        *delta_y_ret = DELTA_SCALAR;
    } else if (cur_dir == MOUSE_DIR_LEFT) {
        *delta_x_ret = -DELTA_SCALAR;
        *delta_y_ret = 0;
    } else if (cur_dir == MOUSE_DIR_UP) {
        *delta_x_ret = 0;
        *delta_y_ret = -DELTA_SCALAR;
    }

    // Update cumulative distance for current direction
    distance += DELTA_SCALAR;
    // Check if we need to change direction
    if (distance >= DISTANCE_MAX) {
        distance = 0;
        cur_dir++;
        if (cur_dir == MOUSE_DIR_MAX) {
            cur_dir = 0;
        }
    }
}

static void do_mash_kb(void)
{
    // Keyboard output: Send key 'a/A' pressed and released
    uint8_t keycode[6] = {HID_KEY_A};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
    vTaskDelay(pdMS_TO_TICKS(50));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
}
static void do_mash_mouse(void) {
    
    // Mouse output: Move mouse cursor in square trajectory
    
    int8_t delta_x;
    int8_t delta_y;
    for (int i = 0; i < (DISTANCE_MAX / DELTA_SCALAR) * 4; i++) {
        // Get the next x and y delta in the draw square pattern
        mouse_draw_square_next_delta(&delta_x, &delta_y);
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, delta_x, delta_y, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static uix::display lcd_display;

using screen_t = uix::screen<rgb_pixel<16>>;
using label_t = label<screen_t::control_surface_type>;
using button_t = vbutton<screen_t::control_surface_type>;
using color_t = color<screen_t::pixel_type>;
using uix_color_t = color<rgba_pixel<32>>;
static void uix_on_flush(const rect16& bounds,
                             const void *bitmap, void* state) {
    lcd_flush(bounds.x1, bounds.y1, bounds.x2, bounds.y2, bitmap);
}
static void uix_on_touch(point16* out_locations,size_t* in_out_locations_size,void* state) {
    if(*in_out_locations_size>0) {
        uint16_t x,y;
        bool pressed = touch_xy(&x,&y);
        if(pressed) {
            out_locations->x=x;
            out_locations->y=y;
            *in_out_locations_size = 1;
        } else {
            *in_out_locations_size = 0;
        }
    }
}
constexpr static const size_t lcd_transfer_size = ((320*240)*2)/10;
static uint8_t* lcd_transfer_buffer = nullptr;
static uint8_t* lcd_transfer_buffer2 = nullptr;

static void lcd_initialize_buffers() {
    lcd_transfer_buffer=(uint8_t*)heap_caps_malloc(lcd_transfer_size,MALLOC_CAP_DMA);
    lcd_transfer_buffer2=(uint8_t*)heap_caps_malloc(lcd_transfer_size,MALLOC_CAP_DMA);
    if(lcd_transfer_buffer==nullptr || lcd_transfer_buffer2==nullptr) {
        puts("Unable to initialize transfer buffers.");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}
void lcd_on_flush_complete() {
    lcd_display.flush_complete();
}
static screen_t main_screen;

static button_t mash_mouse;
static button_t mash_kb;
static button_t mash_gamepad;

void loop(void);
static void loop_task(void* arg) {
    uint32_t ms = pdTICKS_TO_MS(xTaskGetTickCount());
    while(1) {
        if(pdTICKS_TO_MS(xTaskGetTickCount())>ms+200) {
            ms = pdTICKS_TO_MS(xTaskGetTickCount());
            vTaskDelay(5);
        }
        loop();
    }
}
char serial_num[17];
extern "C" void app_main() {
    uint8_t vals[8];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(vals));
    snprintf(serial_num,16,"%02X%02X%02X%02X%02X%02X%02X%02X",vals);
    hid_string_descriptor[3]=serial_num;
    lcd_initialize(lcd_transfer_size);
    lcd_initialize_buffers();
    lcd_rotation(0);
    touch_initialize(TOUCH_THRESH_DEFAULT);
    touch_rotation(0);
    lcd_display.buffer_size(lcd_transfer_size);
    lcd_display.buffer1(lcd_transfer_buffer);
    lcd_display.buffer2(lcd_transfer_buffer2);
    lcd_display.on_flush_callback(uix_on_flush);
    lcd_display.on_touch_callback(uix_on_touch);
    main_screen.dimensions({240,320});
    mash_mouse.bounds(srect16(0,0,199,59).center_horizontal(main_screen.bounds()).offset(0,2));
    mash_mouse.color(uix_color_t::white);
    mash_mouse.back_color(uix_color_t::dark_orange);
    mash_mouse.font_size(30);
    mash_mouse.font(jetBrainsMono);
    mash_mouse.text("mouse!");
    main_screen.register_control(mash_mouse);

    mash_kb.bounds(mash_mouse.bounds().offset(0,mash_mouse.dimensions().height+2));
    mash_kb.color(uix_color_t::white);
    mash_kb.back_color(uix_color_t::dark_green);
    mash_kb.font_size(30);
    mash_kb.font(jetBrainsMono);
    mash_kb.text("k/b!");
    
    main_screen.register_control(mash_kb);
        
    mash_gamepad.bounds(mash_kb.bounds().offset(0,mash_kb.dimensions().height+2));
    mash_gamepad.color(uix_color_t::white);
    mash_gamepad.back_color(uix_color_t::blue);
    mash_gamepad.font_size(30);
    mash_gamepad.font(jetBrainsMono);
    mash_gamepad.text("gamepad!");
    main_screen.register_control(mash_gamepad);
        
    lcd_display.active_screen(main_screen);

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor, // HID configuration descriptor for full-speed and high-speed are the same
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor= hid_configuration_descriptor,
#endif // TUD_OPT_HIGH_SPEED
    };

    TaskHandle_t loop_handle;
    xTaskCreate(loop_task,"loop_task",8192,NULL,10,&loop_handle);
    puts("Installing tinyusb driver.");
    // screen paints, unless this is called:
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    
}
void loop() {
    // keep this commented for now
    // if (tud_mounted()) {
    //     if(mash_mouse.pressed()) {
    //         do_mash_mouse();
    //     }
    //     if(mash_kb.pressed()) {
    //         do_mash_kb();
    //     }
    // }
    // this IS getting called when uncommented
    //puts("loop()");
    lcd_display.update();
}