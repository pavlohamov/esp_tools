/*
 * TPS25751.hpp
 *
 *  Created on: 1 Aug 2026
 *      Author: pavloha
 */

#pragma once


#include "Runnable.hpp"

#include "BQ25798.hpp"

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
	TPS25751(i2c_master_bus_handle_t bus, int addr, gpio_num_t gpio_irq) noexcept;
	virtual ~TPS25751() noexcept;
protected:
	virtual void run() noexcept;
private:
	const i2c_master_bus_handle_t bus_;
	const gpio_num_t gpio_irq_;
	i2c_master_dev_handle_t dev_;
	SemaphoreHandle_t pending_;
	BQ25798 *charger_;

	static void onGpioCb(void* arg) noexcept;

	int readreg(int addr, uint8_t* data, size_t size) noexcept;
	int writereg(int addr, const uint8_t* data, size_t size) noexcept;

	int readreg(int addr, std::vector<uint8_t>& data) noexcept;
	int writereg(int addr, const std::vector<uint8_t>& data) noexcept;

	int sendcmd1(const char* data) noexcept;

	constexpr State decodeState(const uint8_t *data) noexcept;

	int loadPatch(int addr, const uint8_t *data, size_t size);

	void dump();
	void dumpStatus();
	void dumpPowerPath();
	void dumpPdo(int addr = 0x30, const char* text = nullptr, int extraOffset = 0);

	int i2c_read(uint8_t devaddr, uint8_t regaddr, void *data, size_t len);
	int i2c_write(uint8_t devaddr, uint8_t regaddr, const void *data, size_t len);

	int i2c_read_single(uint8_t devaddr, uint8_t regaddr, uint8_t *data, size_t len);
	int i2c_write_single(uint8_t devaddr, uint8_t regaddr, const uint8_t *data, size_t len);
};
