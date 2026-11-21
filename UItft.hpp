/*
 * UItft.hpp
 *
 *  SPI TFT (ST7789) UI on top of BaseUI / LVGL.
 *
 *  Hotplug:
 *   SPI to ST7789 is write-only on most boards (no MISO), so a missing panel
 *   can not be detected by a failed transfer. Two strategies:
 *    - blind (default): every keepalive_ms the panel gets a soft re-init
 *      (SLPOUT/MADCTL/COLMOD/INV/DISPON) and the whole screen is redrawn.
 *      Harmless for a working panel, brings a re-plugged panel back.
 *    - probe (probe_sio = true): SPI runs in 3-wire mode (MOSI bidirectional),
 *      RDDPM (0Ah) is read back; soft re-init only if the panel is not
 *      "sleep out + display on" (i.e. was power cycled / unplugged).
 *   Any esp_lcd error (create/reset/init/draw) -> full re-init with backoff.
 */

#pragma once

#include "UI.hpp"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_types.h"

class TftUI: public BaseUI {
public:
	struct Config {
		spi_host_device_t host = SPI2_HOST;
		bool init_bus = true;          // false if SPI bus is already initialized/shared
		int gpio_sclk = -1;
		int gpio_mosi = -1;
		int gpio_cs = -1;
		int gpio_dc = -1;
		int gpio_rst = -1;             // -1: soft reset (SWRESET)
		int gpio_bl = -1;              // backlight / enable pin, -1: none
		int bl_on_level = 1;
		uint32_t pclk_hz = 20 * 1000 * 1000;

		uint16_t hres = 240;           // LVGL resolution (after swap_xy)
		uint16_t vres = 320;
		bool swap_xy = false;
		bool mirror_x = false;
		bool mirror_y = false;
		bool invert = false;
		int gap_x = 0;
		int gap_y = 0;
		lcd_rgb_element_order_t rgb_order = LCD_RGB_ELEMENT_ORDER_RGB;

		uint16_t fb_lines = 20;        // height of each of 2 partial DMA buffers
		uint32_t keepalive_ms = 2000;  // 0: hotplug check disabled
		bool probe_sio = false;        // read RDDPM via 3-wire SPI (see above)
	};

	explicit TftUI(const Config &cfg) noexcept;
	virtual ~TftUI() noexcept;

protected:
	virtual int create_io() noexcept override;
	virtual int create_panel() noexcept override;
	virtual int configure_panel() noexcept override;
	virtual int initialzie_lvgl() noexcept override;
	virtual bool is_alive() noexcept override;
	virtual void draw() noexcept override {}

	const Config cfg_;

private:
	bool bus_owned_;
	int probe_fails_;

	int soft_reinit() noexcept;
	static void on_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) noexcept;
};
