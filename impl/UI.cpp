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

#define EXAMPLE_LVGL_TICK_PERIOD_MS    5

static bool on_flush_ready(esp_lcd_panel_io_handle_t io_panel, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void on_lvgl_tick(void *arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static inline int64_t now_ms() {
	return esp_timer_get_time() / 1000;
}

BaseUI::BaseUI(uint16_t hres, uint16_t vres) noexcept: Runnable("BaseUi"),
		hres_(hres), vres_(vres), panel_io_(nullptr), panel_(nullptr), lvgl_tick_timer_(nullptr), display_(nullptr),
		need_init_(true), keepalive_ms_(0), fb0_(nullptr), fb1_(nullptr) {
	_lock_init(&lock_);

// todo: this must be executed only once

	static std::atomic<bool> done{false};
	bool expected = false;
	if (done.compare_exchange_strong(expected, true)) {
		lv_init();
		const esp_timer_create_args_t lvgl_tick_timer_args = {
			.callback = on_lvgl_tick,
			.arg = nullptr,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "lvgl_tick",
			.skip_unhandled_events = false,
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
	int first_log = 1;
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
			if (first_log) {
				ESP_LOGE(TAG, "create_panel %d", rv);
				first_log = 0;
			}
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

void BaseUI::invalidate_all() noexcept {
	if (!display_)
		return;
	lock();
	// screen object covers whole display; top/sys layers are redrawn in the same area
	lv_obj_invalidate(lv_display_get_screen_active(display_));
	unlock();
}

void BaseUI::run() noexcept {

	lock();
	int rv = 0;
	if ((rv = create_io())) {
		unlock();
		ESP_LOGE(TAG, "create_io %d", rv);
		return;
	}

	if ((rv = initialzie_lvgl())) {
		ESP_LOGE(TAG, "initialzie_lvgl %d", rv);
		unlock();
		return;
	}

	const esp_lcd_panel_io_callbacks_t cbs = {
		.on_color_trans_done = on_flush_ready,
	};
	ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_, &cbs, display_));
	unlock();

	int64_t keepalive_at = now_ms() + keepalive_ms_;
	while (running()) {

		if (need_init_) {
			initialzie_panel();
			invalidate_all();
			continue;
		}

		if (keepalive_ms_ && now_ms() >= keepalive_at) {
			keepalive_at = now_ms() + keepalive_ms_;
			if (!is_alive()) {
				ESP_LOGW(TAG, "keepalive failed %d -> reinit", rv);
				need_init_ = true;
				continue;
			}
		}

		draw();
		lock();
		uint32_t sleep4_ms = lv_timer_handler();
		unlock();
		sleep4_ms = MAX(sleep4_ms, MIN_DELAY_MS);
		sleep4_ms = MIN(sleep4_ms, MAX_DELAY_MS);
		// todo: replace with queue
		usleep(1000 * sleep4_ms);
	}
}
