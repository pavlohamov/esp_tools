/*
 * UI.hpp
 *
 *  Created on: 14 Aug 2026
 *      Author: pavloha
 */

#pragma once

#include "Runnable.hpp"
#include "UI.hpp"

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>

#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

class OledUI: public BaseUI {
public:
	OledUI(i2c_master_bus_handle_t i2c_bus, uint8_t addr, bool issh1107, int rst_pin = -1) noexcept;
	virtual ~OledUI() noexcept;

protected:
	virtual int create_io() noexcept;
	virtual int create_panel() noexcept;
	virtual int configure_panel() noexcept;
	virtual int initialzie_lvgl() noexcept;
	virtual void draw() noexcept {}
private:
	const i2c_master_bus_handle_t bus_;
	const uint8_t addr_;
	const bool ish1107_;
	const int rst_pin_;
	void *render_buff_;
private:
	static void on_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) noexcept;
};
