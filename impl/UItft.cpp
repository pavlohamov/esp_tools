/*
 * UItft.cpp
 *
 *  Port of the old main/src/uiEngine.c (ST7789 over SPI) to BaseUI.
 */

#include "UItft.hpp"

#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_ops.h"

#include "esp_log.h"
static const char *TAG = "UItft";

// ST7789 "Read Display Power Mode"
#define ST7789_RDDPM          0x0A
#define RDDPM_BOOSTER_ON      0x80
#define RDDPM_SLEEP_OUT       0x10
#define RDDPM_NORMAL_MODE     0x08
#define RDDPM_DISPLAY_ON      0x04
#define RDDPM_EXPECTED        (RDDPM_BOOSTER_ON | RDDPM_SLEEP_OUT | RDDPM_NORMAL_MODE | RDDPM_DISPLAY_ON)

static size_t fb_bytes(const TftUI::Config &cfg) {
	return size_t(cfg.hres) * cfg.fb_lines * sizeof(uint16_t);
}

TftUI::TftUI(const Config &cfg) noexcept :
		BaseUI(cfg.hres, cfg.vres), cfg_(cfg), bus_owned_(false), probe_fails_(0) {
	keepalive_ms_ = cfg.keepalive_ms;
}

TftUI::~TftUI() noexcept {
	// must be released before the bus, BaseUI dtor runs too late for that
	if (panel_) {
		esp_lcd_panel_del(panel_);
		panel_ = nullptr;
	}
	if (panel_io_) {
		esp_lcd_panel_io_del(panel_io_);
		panel_io_ = nullptr;
	}
	if (bus_owned_)
		spi_bus_free(cfg_.host);
}

int TftUI::create_io() noexcept {

	if (cfg_.gpio_bl >= 0) {
		const gpio_config_t bl = {
			.pin_bit_mask = 1ULL << cfg_.gpio_bl,
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
			.hys_ctrl_mode = GPIO_HYS_SOFT_DISABLE,
#endif
		};
		gpio_config(&bl);
		gpio_set_level(gpio_num_t(cfg_.gpio_bl), cfg_.bl_on_level);
	}

	int rv = 0;
	if (cfg_.init_bus) {
		spi_bus_config_t buscfg = {};
		buscfg.sclk_io_num = cfg_.gpio_sclk;
		buscfg.mosi_io_num = cfg_.gpio_mosi;
		buscfg.miso_io_num = -1;
		buscfg.quadwp_io_num = -1;
		buscfg.quadhd_io_num = -1;
		buscfg.max_transfer_sz = int(fb_bytes(cfg_));
		if ((rv = spi_bus_initialize(cfg_.host, &buscfg, SPI_DMA_CH_AUTO))) {
			ESP_LOGE(TAG, "spi_bus_initialize %d", rv);
			return rv;
		}
		bus_owned_ = true;
	}

	esp_lcd_panel_io_spi_config_t io_config = {};
	io_config.cs_gpio_num = gpio_num_t(cfg_.gpio_cs);
	io_config.dc_gpio_num = gpio_num_t(cfg_.gpio_dc);
	io_config.spi_mode = 0;
	io_config.pclk_hz = cfg_.pclk_hz;
	io_config.trans_queue_depth = 10;
	io_config.lcd_cmd_bits = 8;
	io_config.lcd_param_bits = 8;
	io_config.flags.sio_mode = cfg_.probe_sio;

	return esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg_.host, &io_config, &panel_io_);
}

int TftUI::create_panel() noexcept {
	esp_lcd_panel_dev_config_t panel_config = {};
	panel_config.reset_gpio_num = gpio_num_t(cfg_.gpio_rst);
	panel_config.rgb_ele_order = cfg_.rgb_order;
	panel_config.bits_per_pixel = 16;
	return esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_);
}

int TftUI::configure_panel() noexcept {
	int rv = 0;
	if ((rv = esp_lcd_panel_swap_xy(panel_, cfg_.swap_xy)))
		return rv;
	if ((rv = esp_lcd_panel_mirror(panel_, cfg_.mirror_x, cfg_.mirror_y)))
		return rv;
	if ((rv = esp_lcd_panel_set_gap(panel_, cfg_.gap_x, cfg_.gap_y)))
		return rv;
	return esp_lcd_panel_invert_color(panel_, cfg_.invert);
}

int TftUI::initialzie_lvgl() noexcept {
	const size_t sz = fb_bytes(cfg_);
	fb0_ = spi_bus_dma_memory_alloc(cfg_.host, sz, 0);
	fb1_ = spi_bus_dma_memory_alloc(cfg_.host, sz, 0);
	if (!fb0_ || !fb1_) {
		ESP_LOGE(TAG, "no DMA memory for 2x%u bytes", unsigned(sz));
		return ESP_ERR_NO_MEM;
	}

	display_ = lv_display_create(hres_, vres_);
	if (!display_)
		return ESP_ERR_NO_MEM;
	lv_display_set_user_data(display_, this);
	lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(display_, fb0_, fb1_, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(display_, on_flush_cb);
	return 0;
}

/*
 * Re-send init sequence without HW reset: no flicker on a working panel,
 * a freshly plugged (power-on-reset) panel gets fully configured.
 * esp_lcd_panel_init() = SLPOUT(+delay) MADCTL COLMOD RAMCTRL,
 * configure_panel() re-sends MADCTL + INVON/INVOFF, then DISPON.
 * SPI io drains pending color transfers before sending commands.
 */
int TftUI::soft_reinit() noexcept {
	int rv = esp_lcd_panel_init(panel_);
	if (!rv)
		rv = configure_panel();
	if (!rv)
		rv = esp_lcd_panel_disp_on_off(panel_, true);
	return rv;
}

bool TftUI::is_alive() noexcept {

	if (cfg_.probe_sio) {
		uint8_t pm = 0;
		int rv = esp_lcd_panel_io_rx_param(panel_io_, ST7789_RDDPM, &pm, 1);
		if (!rv && (pm & RDDPM_EXPECTED) == RDDPM_EXPECTED) {
			if (probe_fails_) {
				ESP_LOGI(TAG, "panel is back");
				probe_fails_ = 0;
			}
			return true;
		}
		if (!probe_fails_++)
			ESP_LOGW(TAG, "panel lost (rv %d pm 0x%02X)", rv, pm);
	}

	return !soft_reinit();
}

void TftUI::on_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) noexcept {
	TftUI *ui = static_cast<TftUI*>(lv_display_get_user_data(disp));

	// SPI LCD is big-endian
	lv_draw_sw_rgb565_swap(px_map, lv_area_get_size(area));

	int rv = esp_lcd_panel_draw_bitmap(ui->panel_, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
	if (rv) {
		// transfer was not queued -> no on_color_trans_done, release LVGL ourselves
		ui->need_init_ = true;
		lv_display_flush_ready(disp);
	}
}
