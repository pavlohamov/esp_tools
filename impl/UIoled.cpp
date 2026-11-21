/*
 * UI.cpp
 *
 *  Created on: 14 Aug 2026
 *      Author: pavloha
 */

#include "UIoled.hpp"

#include <sys/param.h>
#include "esp_heap_caps.h"

#include "esp_lcd_sh1107.h"
#include "esp_lcd_panel_vendor.h"

#include "esp_log.h"
static const char *TAG = "UIoled";

#define EXAMPLE_LVGL_PALETTE_SIZE      8

static bool on_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}


OledUI:: OledUI(i2c_master_bus_handle_t i2c_bus, uint8_t addr, bool ish1107, int rst_pin) noexcept :
		BaseUI(128, 64), bus_(i2c_bus), addr_(addr), ish1107_(ish1107), rst_pin_(rst_pin) {

}
OledUI::~OledUI() noexcept {

}

int OledUI::create_io() noexcept {

	esp_lcd_panel_io_i2c_config_t io_config = {
		.dev_addr = addr_,
		.scl_speed_hz = 400 * 1000,
		.control_phase_bytes = 1,
		.dc_bit_offset = 6,
		.lcd_cmd_bits = 8,
		.lcd_param_bits = 8,
		.on_color_trans_done = nullptr,
		.user_ctx = nullptr,
		.flags = {
			.dc_low_on_data = 0,
			.disable_control_phase = 0,
		},
		.transaction_timeout_ms = 25,
	};

	if (ish1107_) {
		io_config.dc_bit_offset = 0;
		io_config.flags.disable_control_phase = 1;
	}
	return esp_lcd_new_panel_io_i2c(bus_, &io_config, &panel_io_);
}

int OledUI::create_panel() noexcept {

	esp_lcd_panel_dev_config_t panel_config = {
		.bits_per_pixel = 1,
		.reset_gpio_num = gpio_num_t(rst_pin_),
	};

	int rv = 0;
	if (ish1107_) {
#if 0
// todo: panel raw bitmap has a bug
--ptr = color_data + i * x_end;
++ptr = color_data + i * size;
#endif
		esp_lcd_panel_sh1107_config_t sh1107_config = {
			.contrast = 0x7F,
			.offset = 0,
		};
		panel_config.vendor_config = &sh1107_config;
		rv = esp_lcd_new_panel_sh1107(panel_io_, &panel_config, &panel_);
		if (!rv)
			rv = esp_lcd_panel_invert_color(panel_, true);
	} else {
		esp_lcd_panel_ssd1306_config_t ssd1306_config = {
			.height = vres_,
		};
		panel_config.vendor_config = &ssd1306_config;
		rv = esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_);
	}
	return rv;
}


int OledUI::configure_panel() noexcept {
	esp_lcd_panel_set_gap(panel_, 2, 0);
	esp_lcd_panel_swap_xy(panel_, 0);
	esp_lcd_panel_mirror(panel_, 0, 0);
	return 0;
}

int OledUI::initialzie_lvgl() noexcept {

    const size_t draw_buffer_sz = (hres_ * vres_ / 8 + EXAMPLE_LVGL_PALETTE_SIZE);
    fb0_ = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    fb1_ = heap_caps_calloc(1, draw_buffer_sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(fb0_);
    assert(fb1_); // here fb1_ is used as rendering buffer

    memset(fb0_, 0xff, draw_buffer_sz);
    memset(fb1_, 0xff, draw_buffer_sz);

    display_ = lv_display_create(hres_, vres_);
    lv_display_set_user_data(display_, this);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display_, fb0_, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display_, on_flush_cb);
    return 0;
}

void OledUI::on_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) noexcept {
	OledUI *ui = (OledUI*)lv_display_get_user_data(disp);
    esp_lcd_panel_handle_t panel_handle = ui->panel_;

    // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a palette. Skip the palette here
    // More information about the monochrome, please refer to https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
    px_map += EXAMPLE_LVGL_PALETTE_SIZE;

    const uint16_t hor_res = lv_display_get_physical_horizontal_resolution(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    uint8_t *fb = (uint8_t*)ui->fb1_;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            bool chroma_color = (px_map[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));

            uint8_t *buf = fb + hor_res * (y >> 3) + (x);
            if (chroma_color) {
                (*buf) &= ~(1 << (y % 8));
            } else {
                (*buf) |= (1 << (y % 8));
            }
        }
    }

    int rv = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, ui->fb1_);
	if (rv) {
		ui->need_init_ = true;
		on_flush_ready(ui->panel_io_, nullptr, ui->display_);
	}
}


