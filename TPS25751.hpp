/*
 * TPS25751.hpp
 *
 *  Created on: 1 Aug 2026
 *      Author: pavloha
 */

#pragma once


#include "MutexFreeRtos.hpp"
#include "Runnable.hpp"

#include "BQ25798.hpp"

#include "usb_pd.hpp"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include <cstdint>
#include <vector>


typedef struct QueueDefinition* QueueHandle_t;
typedef QueueHandle_t SemaphoreHandle_t;

class TPS25751 : public Runnable {
public:
	enum State {
		PTCH,
		BOOT,
		APP,
		Unknown,
	};
	TPS25751(i2c_master_bus_handle_t bus, uint8_t addr, gpio_num_t gpio_irq, uint8_t bqaddr = 0x6b) noexcept;
	virtual ~TPS25751() noexcept;

	BQ25798 *getCharger() { return charger_.get(); }

	bool autoneg_set(const usbpd::source_capabilities::selection& sel);

	bool read_rx_source_caps(usbpd::source_capabilities& out);
	bool read_rx_sink_caps(usbpd::source_capabilities& out);

	bool read_tx_source_caps(usbpd::source_capabilities& out);
	bool read_tx_sink_caps(usbpd::source_capabilities& out);

	bool read_active_pdo(usbpd::pdo& pdo);

protected:
	virtual void run() noexcept;
private:
	const i2c_master_bus_handle_t bus_;
	const gpio_num_t gpio_irq_;
	const uint8_t bqaddr_;
	i2c_master_dev_handle_t dev_;
	SemaphoreHandle_t pending_;
	MutexRecursiveFr lock_;
	std::unique_ptr<BQ25798> charger_;

	static void onGpioCb(void* arg) noexcept;

	bool read_caps(int regadr, usbpd::source_capabilities& out, int extraOffset = 0);

	int readreg(int addr, uint8_t* data, size_t size) noexcept;
	int writereg(int addr, const uint8_t* data, size_t size) noexcept;

	int readreg(int addr, std::vector<uint8_t>& data) noexcept;
	int writereg(int addr, const std::vector<uint8_t>& data) noexcept;

	int write_4CC(const char* cc4, const void* tx = nullptr, size_t tx_len = 0, void* rx = nullptr, size_t rx_len = 0) noexcept;
	int write_4CC(const char* cc4, const void* tx, size_t tx_len, std::vector<uint8_t>& data) noexcept;

	constexpr State decodeState(const uint8_t *data) noexcept;

	int initialize();

	int loadPatch(int addr, const uint8_t *data, size_t size);

	int i2c_read(uint8_t devaddr, uint8_t regaddr, void *data, size_t len);
	int i2c_write(uint8_t devaddr, uint8_t regaddr, const void *data, size_t len);

	int i2c_read_single(uint8_t devaddr, uint8_t regaddr, uint8_t *data, size_t len);
	int i2c_write_single(uint8_t devaddr, uint8_t regaddr, const uint8_t *data, size_t len);
};
