/*
 * UI.hpp
 *
 *  Created on: 14 Aug 2026
 *      Author: pavloha
 */

#pragma once

#include "Runnable.hpp"

#include <atomic>
#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>

#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

class BaseUI: public Runnable {
public:
	BaseUI(uint16_t hres, uint16_t vres) noexcept;
	virtual ~BaseUI() noexcept;

	lv_display_t *display() noexcept {
		return display_;
	}

	void requesReinit() { need_init_ = true; }

	inline void lock() { _lock_acquire_recursive(&lock_); }
	inline bool try_lock() { return _lock_try_acquire_recursive(&lock_) == 0; }
	inline void unlock() { _lock_release_recursive(&lock_); }

protected:
	virtual void run() noexcept;

	virtual int create_io() noexcept = 0;
	virtual int create_panel() noexcept = 0;
	virtual int configure_panel() noexcept = 0;
	virtual int initialzie_lvgl() noexcept = 0;
	virtual void draw() noexcept = 0;

	virtual bool is_alive() noexcept { return !need_init_; }

	void invalidate_all() noexcept;

private:
	int initialzie_panel() noexcept;

protected:
	const uint16_t hres_;
	const uint16_t vres_;
	_lock_t lock_;
	esp_lcd_panel_io_handle_t panel_io_;
	esp_lcd_panel_handle_t panel_;
	esp_timer_handle_t lvgl_tick_timer_;
	lv_display_t *display_;
	std::atomic<bool> need_init_;
	uint32_t keepalive_ms_;

	void *fb0_;
	void *fb1_;
};
