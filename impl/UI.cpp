/*
 * UI.cpp
 *
 *  Created on: 14 Aug 2026
 *      Author: pavloha
 */

#include "UI.hpp"

#include <sys/param.h>
#include "esp_heap_caps.h"


#include "esp_log.h"
static const char *TAG = "UI";


#define MAX_DELAY_MS 500
#define MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ

#define EXAMPLE_LVGL_PALETTE_SIZE      8

#define EXAMPLE_LVGL_TICK_PERIOD_MS    5

static bool on_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void on_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}


BaseUI::BaseUI(uint16_t hres, uint16_t vres) noexcept: Runnable("BaseUi"),
		hres_(hres), vres_(vres), panel_io_(nullptr), panel_(nullptr), lvgl_tick_timer_(nullptr), display_(nullptr), fb0_(nullptr), fb1_(nullptr), need_init_(true) {
	_lock_init(&lock_);

// todo: this must be executed only once

	static std::atomic<bool> done{false};
	bool expected = false;
	if (done.compare_exchange_strong(expected, true)) {
		lv_init();
		const esp_timer_create_args_t lvgl_tick_timer_args = {
			.callback = on_lvgl_tick,
			.name = "lvgl_tick"
		};
		ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer_));
		ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer_, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
	}
}

BaseUI::~BaseUI() noexcept {

	if (fb1_)
		free(fb1_);

	if (fb0_)
		free(fb0_);

	if (display_)
		lv_display_delete(display_);

	if (lvgl_tick_timer_)
		ESP_ERROR_CHECK(esp_timer_delete(lvgl_tick_timer_));

	if (panel_)
		esp_lcd_panel_del(panel_);

	if (panel_io_)
		esp_lcd_panel_io_del(panel_io_);
}

int BaseUI::initialzie_panel() noexcept {
	int rv = 0;
	size_t wait4 = 0;
	while (need_init_ && running()) {
		usleep(1000 * wait4);
		wait4 = (wait4 << 1) + 1;
		if (wait4 > 2000)
			wait4 = 2000;

		if (panel_) {
			esp_lcd_panel_del(panel_);
			panel_ = nullptr;
		}

		if ((rv = create_panel())) {
			ESP_LOGE(TAG, "create_panel %d", rv);
			continue;
		}

		if ((rv = esp_lcd_panel_reset(panel_))) {
			continue;
		}
		if ((rv = esp_lcd_panel_init(panel_))) {
			continue;
		}
		if ((rv = esp_lcd_panel_disp_on_off(panel_, 1))) {
			continue;
		}
		need_init_ = false;
		break;
	}
	if (!need_init_)
		configure_panel();
	return rv;
}

void BaseUI::run() noexcept {

	_lock_acquire(&lock_);
	int rv = 0;
	if ((rv = create_io())) {
		_lock_release(&lock_);
		ESP_LOGE(TAG, "create_io %d", rv);
		return;
	}

	if ((rv = initialzie_lvgl())) {
		ESP_LOGE(TAG, "initialzie_lvgl %d", rv);
		_lock_release(&lock_);
		return;
	}

	const esp_lcd_panel_io_callbacks_t cbs = {
		.on_color_trans_done = on_flush_ready,
	};
	ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, display_));
	_lock_release(&lock_);

	while (running()) {

		if (need_init_) {
			initialzie_panel();
			continue;
		}

		draw();
		_lock_acquire(&lock_);
		uint32_t sleep4_ms = lv_timer_handler();
		_lock_release(&lock_);
		sleep4_ms = MAX(sleep4_ms, MIN_DELAY_MS);
		sleep4_ms = MIN(sleep4_ms, MAX_DELAY_MS);
		// todo: replace with queue
		usleep(1000 * sleep4_ms);
	}
}

#include "esp_lcd_sh1107.h"
#include "esp_lcd_panel_vendor.h"

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

void OledUI::on_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
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


