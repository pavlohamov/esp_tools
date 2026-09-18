/*
 * TPS25751.cpp
 *
 *  Created on: 1 Aug 2026
 *      Author: pavloha
 */


#include "TPS25751.hpp"

#include <endian.h>
#include <mutex>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_timer.h"

#include "esp_log.h"
static const char *TAG = "TPS25751";

#define BQ25798_ADR 0x6b
#define I2C_READ_MAX 10
#define I2C_WRITE_MAX 18

struct regpair {
	const uint8_t addr;
	const uint16_t len;
};


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif

#define DEFAULT_I2C_TOUT_MS 50

extern "C" {
	extern const uint8_t _binary_TPS25751_bin_start[];
	extern const uint8_t _binary_TPS25751_bin_end[];
}

static const uint8_t s_clearIrq[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const uint8_t s_enableCMD1irq[] = { 0x00, 0x00, 0x00, 0x40,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x01 };


static const regpair s_regmap[] = {
	{ 0x03, 4 }, // Mode
	{ 0x06, 8 }, // Customer Use
	{ 0x08, 4 }, // Command Register for I2C1
	{ 0x09, 64 }, // Data Register for CMD1
	{ 0x14, 11 }, // Interrupt Event for I2C1
	{ 0x16, 11 }, // Interrupt Mask for I2C1
	{ 0x18, 11 }, // Interrupt Clear for I2C1
	{ 0x1A, 5 }, // Status
	{ 0x26, 5 }, // Power Path Status
	{ 0x28, 16 }, // Port Configuration
	{ 0x29, 4 }, // Port Control
	{ 0x2D, 5 }, // Boot Flags
	{ 0x30, 53 }, // Received Source Capabilities
	{ 0x31, 53 }, // Received Sink Capabilities
	{ 0x32, 63 }, // Transmit Source Capabilities
	{ 0x33, 53 }, // Transmit Sink Capabilities
	{ 0x34, 6 }, // Active PDO Contract
	{ 0x35, 12 }, // Active RDO Contract
	{ 0x37, 24 }, // Autonegotiate Sink
	{ 0x3F, 2 }, // Power Status
	{ 0x40, 4 }, // PD Status
	{ 0x5C, 49 }, // IO Config
	{ 0x69, 4 }, // Type C State
	{ 0x6A, 13 }, // ADC Results
	{ 0x70, 1 }, // Sleep Control Register
	{ 0x72, 8 }, // GPIO Status
	{ 0x98, 11 }, // Liquid Detection Config
};

struct AutonegotiateSink {
	uint32_t autoPrioLowVolt:1;
	uint32_t noUsbSuspend:1;
	uint32_t autoComputeSinkPower:1;
	uint32_t noCapMissmatch:1;
	uint32_t autoComputeSinkMinVolt:1;
	uint32_t autoComputeSinkMaxVolt:1;
	uint32_t autoDisableSink:1;
	uint32_t :5;
	uint32_t autoNegMaxCurrent:10; // 250mw
	uint32_t autoNegSinkMinPower:10; // 250mw
	uint32_t autoNegMaxVolt:10; // 50mw
	uint32_t autoNegMinVolt:10; // 50mw
	uint32_t autoNegMissmatchPower:10; // 250mw ignore if noCapMissmatch
	uint32_t :2;
	uint32_t ppsEnaSink:1;
	uint32_t ppsReqInterval:2;// 0 -8s, 3 -1s
	uint32_t ppsSourceMode:1; // 0 -cc, 1- cv
	uint32_t ppsReqFullVolt:1;
	uint32_t ppsDisSinkNotAPDO:1;
	uint32_t :26;
	uint32_t ppsOpCurrent:7; // 50ma
	uint32_t :2;
	uint32_t ppsOpVoltage:11; // 20mV
	uint32_t :32;
	uint32_t :32;
	uint32_t :12;
} __attribute__((packed));

struct PBMs_Data_Out { // 6 bytes
	uint32_t size;
	uint16_t i2c_addr:7; // 0x30 prefered
	uint16_t :1;
	uint16_t i2c_tout:6; // 0x31 prefered == 100ms
	uint16_t :2;
} __attribute__((packed));

struct Status_Out {
	uint16_t plugPresent:1; // plugged
	uint16_t connectionState:3; // 0: no, 1: disabled, 2: audio, 3: debug, 4: no Ra, 6: Conn no Ra, 7: Conn + Ra
	uint16_t plugOrientation:1; // 0: UU, 1: UD
	uint16_t portRole:1; // 0: sink, 1: source
	uint16_t dataRole:1; // 0: UFP, 1: DFP
	uint16_t :13;
	uint16_t vbusStatus:2; // 0: < 0.8V, 1: [4.75; 5.5], 2: ok, 3: not in spec
	uint16_t usbHostPresent:2; // 1 - no data, 2 - no PD, 3 - host here
	uint16_t actingLegacy:2; // legacy: 1 - sink, 2 - source, 3 - sink dead-battery
	uint16_t :1;
	uint16_t bist:1; // BIST in progress
	uint16_t :2;
	uint16_t socAckTout:1; // SOC response timed out
} __attribute__((packed));


struct PowerPathStatus_Out {
	uint16_t ppCable1:2; // 0: dis, 1: currently dis, 2: cc1 ena, 3: cc2 ena
	uint16_t :4;
	uint16_t ppSwitch1:3; // 0: dis, 1: currently dis (fault), 2: pp1 ena,
	uint16_t :3;
	uint16_t ppSwitch3:3; // 0: dis, 1: currently dis (fault), 2: pp3 ena (out), 3: pp3 ena (in)
	uint16_t :13;
	uint16_t pp1overcurrent:1; // PP_5V1 overcurrent indicator.
	uint16_t :5;
	uint16_t ppCable1overcurrent:1; // Asserted if overcurrent on PP_CABLE1 (VCONN).
	uint16_t :3;
	uint16_t powerSource:2; // 1: VIN_3V3, 2: VBUS
} __attribute__((packed));


struct PortControl {
	uint16_t current:2;
	uint16_t :2;
	uint16_t processSwap2Sink:1;
	uint16_t initiateSwap2Sink:1;
	uint16_t processSwap2Source:1;
	uint16_t initiateSwap2Source:1;
	uint16_t :4;
	uint16_t processSwap2UFP:1;
	uint16_t initiateSwap2UFP:1;
	uint16_t processSwap2DFP:1;
	uint16_t initiateSwap2DFP:1;
	uint16_t autoIDrequest:1;
	uint16_t :2;
	uint16_t unconstrainedPower:1;
	uint16_t enableCurrentMonitor:1;
	uint16_t :3;
	uint16_t r15kPresent:1;
	uint16_t dcdEna:1;
	uint16_t dcdAdvertEna:3;
	uint16_t :1;
	uint16_t dcdChargerEna:2;
} __attribute__((packed));

static_assert(sizeof(PortControl) == 4);


TPS25751::TPS25751(i2c_master_bus_handle_t bus, uint8_t addr, gpio_num_t gpio_irq, uint8_t bqaddr):
		bus_(bus), gpio_irq_(gpio_irq), bqaddr_(bqaddr), dev_(nullptr), pending_(xSemaphoreCreateBinary()), lock_() {

	const i2c_device_config_t i2_conf = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = 400 * 1000,
		.scl_wait_us = 0,
		.flags = {
			.disable_ack_check = 0,
		}
	};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &i2_conf, &dev_));

	const gpio_config_t cfg = {
		.pin_bit_mask = BIT64(gpio_irq_),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_NEGEDGE,
	};
	ESP_ERROR_CHECK(gpio_config(&cfg));
	if (gpio_isr_handler_add(gpio_irq_, onGpioCb, this)) {
		ESP_ERROR_CHECK(gpio_install_isr_service(0));
		ESP_ERROR_CHECK(gpio_isr_handler_add(gpio_irq_, onGpioCb, this));
	}
	ESP_ERROR_CHECK(gpio_intr_enable(gpio_irq_));
}

TPS25751::~TPS25751() noexcept {
	ESP_ERROR_CHECK(gpio_intr_disable(gpio_irq_));
	ESP_ERROR_CHECK(gpio_isr_handler_remove(gpio_irq_));
	ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_));
	vSemaphoreDelete(pending_);
}


bool TPS25751::read_caps(int regadr, usbpd::source_capabilities& out, int extraOffset) {

	std::lock_guard<MutexRecursiveFr> guard(lock_);

	std::vector<uint8_t> data;
	int rv = readreg(regadr, data);
	if (rv) {
		ESP_LOGE(TAG, "not read %d", rv);
		return false;
	}
	const auto ptr = data.data();
	const uint8_t len = ptr[0];
	if (len < 2) {
		return false;
	}

	const uint8_t n = ptr[1] & 0x07;
	if (n == 0 || (1u + 1u + 4u * n) > len) {
		return false;
	}

	out.clear();
	for (uint8_t i = 0; i < n; ++i) {
		const uint8_t* p = &ptr[2 + extraOffset + 4 * i];
		out.add(usbpd::pdo{ uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24 });
	}
	return true;
}


bool TPS25751::autoneg_set(const usbpd::source_capabilities::selection& sel) {
	std::vector<uint8_t> data;
	if (readreg(0x37, data))
		return false;
	AutonegotiateSink *as = (AutonegotiateSink*)(data.data() + 1);

	as->autoComputeSinkMaxVolt = 0;
	writereg(0x37, (uint8_t*)as, sizeof(*as));
	as->autoNegMaxVolt = sel.voltage.milli() / 50;
	//				as->autoNegSinkMinPower = 5000 * 3 / 250;
	////				as->autoNegMissmatchPower = 100000 / 250;
	as->noCapMissmatch = 0;
	writereg(0x37, (uint8_t*)as, sizeof(*as));
	write_4CC("ANeg");
	return true;
}

bool TPS25751::read_rx_source_caps(usbpd::source_capabilities& out) {
	return read_caps(0x30, out);
}

bool TPS25751::read_rx_sink_caps(usbpd::source_capabilities& out) {
	return read_caps(0x31, out);
}

bool TPS25751::read_tx_source_caps(usbpd::source_capabilities& out) {
	return read_caps(0x32, out, 2);
}

bool TPS25751::read_tx_sink_caps(usbpd::source_capabilities& out) {
	return read_caps(0x33, out);
}

bool TPS25751::read_active_pdo(usbpd::pdo& pdo) {
	std::vector<uint8_t> data;
	if (readreg(0x34, data))
		return false;
	pdo = usbpd::pdo(*(uint32_t*)(data.data() + 1));
	return true;
}

bool TPS25751::is_pugged() {
	uint8_t buff[sizeof(Status_Out) + 1];
	if (readreg(0x1A, buff, sizeof(buff)))
		return false;
	const Status_Out* stt = (Status_Out*)(buff + 1);
	return stt->plugPresent;
}

int TPS25751::initialize() {

	std::vector<uint8_t> data;
	const uint64_t start_at = esp_timer_get_time();
	bool wait = false;
	do {
		if (wait)
			vTaskDelay(pdMS_TO_TICKS(150));
		std::lock_guard<MutexRecursiveFr> guard(lock_);
		wait = true;
		int rv = readreg(3, data);
		if (rv)
			continue;

		const State st = decodeState(data.data() + 1);
		if (st == APP) {
#if 01
			break;
#else
			ESP_LOGW(TAG, "APP. Send GAID. May restart if dead battery");
			write_4CC("GAID");
			continue;
#endif
		}
		ESP_LOGW(TAG, "state %d '%.4s'", st, data.data() + 1);

		rv = writereg(0x16, s_enableCMD1irq, sizeof(s_enableCMD1irq));
		if (rv) {
			ESP_LOGE(TAG, "ena CMD1 interrupt %d", rv);
			continue;
		}

		static const PBMs_Data_Out pbdata = {
			.size = _binary_TPS25751_bin_end - _binary_TPS25751_bin_start,
			.i2c_addr = 0x30,
			.i2c_tout = 0x3F,
		};

		rv = write_4CC("PBMs", &pbdata, sizeof(pbdata), data);
		if (rv) {
			ESP_LOGE(TAG, "PBMs err");
//			write_4CC("GAID");
			continue;
		}

		if (data.data()[5] != pbdata.i2c_addr) {
			ESP_LOGE(TAG, "address 0x%X != 0x%X", data.data()[5], pbdata.i2c_addr);
			continue;
		}

		if (i2c_master_probe(bus_, pbdata.i2c_addr, 50)) {
			ESP_LOGE(TAG, "not probed 0x%x", pbdata.i2c_addr);
			continue;
		}

		rv = loadPatch(pbdata.i2c_addr, _binary_TPS25751_bin_start, _binary_TPS25751_bin_end - _binary_TPS25751_bin_start);
		if (rv) {
			ESP_LOGE(TAG, "load patch %d", rv);
			continue;
		}

		rv = write_4CC("PBMc", nullptr, 0, data);
		if (rv) {
			ESP_LOGE(TAG, "PBMc err");
			continue;
		}

		vTaskDelay(pdMS_TO_TICKS(20));

		rv = readreg(3, data);
		if (rv) {
			ESP_LOGE(TAG, "state read %d", rv);
			continue;
		}

		if (decodeState(data.data() + 1) == APP) {
			writereg(0x16, s_enableCMD1irq, sizeof(s_enableCMD1irq));
			writereg(0x18, s_clearIrq, sizeof(s_clearIrq));
			if (!readreg(0x2D, data)) {
				if (data.data()[1] & 4)
					write_4CC("DBfg");
			}
			break;
		}

	} while (running());

	const uint32_t loading_took = (esp_timer_get_time() - start_at) / 1000UL;
	if (loading_took > 50)
		ESP_LOGI(TAG, "loading_took %d", loading_took);

	if (bqaddr_ && !charger_) {
		auto writer = [&](uint8_t i2c_addr, uint8_t reg, const void *data, size_t size) {
			return i2c_write(i2c_addr, reg, data, size);
		};
		auto reader = [&](uint8_t i2c_addr, uint8_t reg, void *data, size_t size) {
			return i2c_read(i2c_addr, reg, data, size);
		};
		charger_ = std::make_unique<BQ25798>(bqaddr_, writer, reader);
	}

	return 0;
}

void TPS25751::run() noexcept {

//	esp_log_level_set("TPS25751", ESP_LOG_VERBOSE);
	Status_Out status{};
	status.socAckTout = 1; // just to force reload

	int rv = 0;
	int64_t lastsinc = 0;
	while (running()) {
		initialize();

		vTaskDelay(pdMS_TO_TICKS(100));
		std::vector<uint8_t> data;
		if (!readreg(0x1A, data)) {
			const Status_Out* stt = (Status_Out*)(data.data() + 1);
			if (memcmp(&status, stt, sizeof(status))) {
				ESP_LOGI(TAG, "Status: [%c] %d %s %s %cFP VB %d HOS %d leg %d bist %d tout %d",
						stt->plugPresent ? '*' : ' ',
						stt->connectionState,
						stt->plugOrientation ? "\\/" : "/\\",
						stt->portRole ? "src" : "snk",
						stt->dataRole ? 'D' : 'U',
						stt->vbusStatus,
						stt->usbHostPresent,
						stt->actingLegacy,
						stt->bist,
						stt->socAckTout);
				memcpy(&status, stt, sizeof(status));
			}
		}
		if (!readreg(0x6A, data)) {
			const uint8_t *ptr = data.data() + 1;
			ESP_LOGV(TAG, "ADC: %d %d %d %d   %d %d %d %d    %d %d", ptr[0], ptr[1], ptr[2], ptr[3],   ptr[5], ptr[7], ptr[8], ptr[9],   ptr[10], ptr[11]);
		}
		if (!readreg(0x29, data)) {
			PortControl* pc = (PortControl*)(data.data() + 1);
			if (!pc->enableCurrentMonitor) {
				pc->enableCurrentMonitor = 1;
				writereg(0x29, (uint8_t*)pc, sizeof(*pc));
			}
		}

		if (!readreg(0x26, data)) {
			const PowerPathStatus_Out* stt = (PowerPathStatus_Out*)(data.data() + 1);
			ESP_LOGV(TAG, "C S1 S3 %d %d %d %d", stt->ppCable1, stt->ppSwitch1, stt->ppSwitch3, stt->powerSource);
		}

	}
}

void TPS25751::onGpioCb(void* arg) noexcept {
	TPS25751 *me = static_cast<TPS25751*>(arg);
	BaseType_t woken;
	xSemaphoreGiveFromISR(me->pending_, &woken);
	portYIELD_FROM_ISR(woken);
}

int TPS25751::writereg(int addr, const uint8_t* data, size_t size) noexcept {
	const regpair *pair = nullptr;
	for (size_t i = 0; i < ARRAY_SIZE(s_regmap); ++i) {
		if (addr != s_regmap[i].addr)
			continue;
		pair = s_regmap + i;
		break;
	}

	if (!pair) {
		ESP_LOGE(TAG, "unkown reg 0x%2X", addr);
		return -EINVAL;
	}

	if (size > pair->len) {
		ESP_LOGE(TAG, "reg 0x%2X len %d but %d given", addr, pair->len, size);
		return -EINVAL;
	}

	ESP_LOGV(TAG, "Write: [%2X] %d:", addr, size);
	ESP_LOG_BUFFER_HEXDUMP(TAG, data, size, ESP_LOG_VERBOSE);

	uint8_t count = size;
	i2c_master_transmit_multi_buffer_info_t mul[] = {
		{ .write_buffer = (uint8_t*)&addr, .buffer_size = 1 },
		{ .write_buffer = &count, .buffer_size = 1 },
		{ .write_buffer = data, .buffer_size = size },
	};

	return i2c_master_multi_buffer_transmit(dev_, mul, ARRAY_SIZE(mul), DEFAULT_I2C_TOUT_MS);
}

int TPS25751::readreg(int addr, uint8_t* data, size_t size) noexcept {
	return i2c_master_transmit_receive(dev_, (uint8_t*)&addr, 1, data, size, DEFAULT_I2C_TOUT_MS);
}


int TPS25751::writereg(int addr, const std::vector<uint8_t>& data) noexcept {
	return writereg(addr, data.data(), data.size());
}

int TPS25751::readreg(int addr, std::vector<uint8_t>& data) noexcept {

	const regpair *pair = nullptr;
	for (size_t i = 0; i < ARRAY_SIZE(s_regmap); ++i) {
		if (addr != s_regmap[i].addr)
			continue;
		pair = s_regmap + i;
		break;
	}
	if (!pair) {
		ESP_LOGE(TAG, "unkown reg 0x%2X", addr);
		return -EINVAL;
	}

	data.resize(pair->len + 1);
	return readreg(addr, data.data(), data.size());
}


int TPS25751::write_4CC(const char* cc4, const void* tx, size_t tx_len, std::vector<uint8_t>& data) noexcept {
	data.resize(64);
	return write_4CC(cc4, tx, tx_len, data.data(), data.size());
}

int TPS25751::write_4CC(const char* cc4, const void* tx, size_t tx_len, void* rx, size_t rx_len) noexcept {

	std::lock_guard<MutexRecursiveFr> guard(lock_);

	int rv = 0;
	if (tx && tx_len) {
		rv = writereg(0x09, (const uint8_t*)tx, tx_len);
		if (rv) {
			ESP_LOGE(TAG, "4CC '%.4s' data write %d", cc4, rv);
			return rv;
		}
	}

	writereg(0x18, s_clearIrq, sizeof(s_clearIrq));
	xSemaphoreTake(pending_, 0);
	rv = writereg(0x08, (const uint8_t*)cc4, 4);
	if (rv) {
		ESP_LOGE(TAG, "4CC '%.4s' write %d", cc4, rv);
		return rv;
	}

	uint8_t ack[5];
	int done = xSemaphoreTake(pending_, pdMS_TO_TICKS(50));
	if (!done) { // command not yet executed
		if ((rv = readreg(0x08, ack, sizeof(ack)))) {
			ESP_LOGE(TAG, "4CC '%.4s' ack read %d", cc4, rv);
			return rv;
		}
		if (ack[0] != 4) {
			ESP_LOGE(TAG, "4CC '%.4s' ack invalid len %d", cc4, ack[0]);
			return -1;
		}
		if (ack[1] == '!') {
			ESP_LOGE(TAG, "4CC '%.4s' nack '%.4s'", cc4, ack + 1);
			return -1;
		}
		done = xSemaphoreTake(pending_, pdMS_TO_TICKS(250)); // continue waiting
	}

	if (!done) {
		ESP_LOGE(TAG, "4CC '%.4s' timeout", cc4);
		return -ETIMEDOUT;
	}

	bool ackchek = false;
	if (!rx || !rx_len) {
		rx = ack;
		rx_len = sizeof(ack);
		ackchek = true;
	}

	if ((rv = readreg(0x09, (uint8_t*)rx, rx_len))) {
		ESP_LOGE(TAG, "4CC '%.4s' read %d", cc4, rv);
	}

	if (ackchek && ack[1])
		ESP_LOGE(TAG, "4CC '%.4s' ack %d %d %d %d %d", cc4, ack[0], ack[1], ack[2], ack[3], ack[4]);

	return ackchek ? -ack[1] : 0;
}

constexpr TPS25751::State TPS25751::decodeState(const uint8_t *data) noexcept {
	static const uint8_t codes[][5] = {
		[PTCH] = "PTCH",
		[BOOT] = "BOOT",
		[APP] = "APP ",
	};
	State st = Unknown;

	const uint32_t value = *(uint32_t*)data;
	for (size_t i = 0; i < ARRAY_SIZE(codes); ++i) {
		const uint32_t* code32 = (uint32_t*)codes[i];
		if (value != *code32)
			continue;
		st = static_cast<State>(i);
		break;
	}
	return st;
}


int TPS25751::loadPatch(int addr, const uint8_t *data, size_t size) {

	const i2c_device_config_t i2_conf = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = addr,
		.scl_speed_hz = 400 * 1000,
		.scl_wait_us = 0,
		.flags = {
			.disable_ack_check = 0,
		}
	};

	i2c_master_dev_handle_t write_dev;
	if (i2c_master_bus_add_device(bus_, &i2_conf, &write_dev)) {
		ESP_LOGE(TAG, "can't add dev");
		return -1;
	}

	size_t written = 0;
	int rv = 0;
	while (written < size) {
		size_t towrite = size - written;
		if (towrite > 4095)
			towrite = 4095;
		rv = i2c_master_transmit(write_dev, data + written, towrite, 100);
		if (rv) {
			ESP_LOGE(TAG, "%zu/%zu err %d", written, size, rv);
			break;
		}
		written += towrite;
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	ESP_ERROR_CHECK(i2c_master_bus_rm_device(write_dev));

	return 0;
}

int TPS25751::i2c_read(uint8_t devaddr, uint8_t regaddr, void *data, size_t len) {
	uint8_t *ptr = (uint8_t*)data;
	size_t read = 0;
	while (read < len) {
		int willread = len - read;
		if (willread > I2C_READ_MAX)
			willread = I2C_READ_MAX;
		int rv = i2c_read_single(devaddr, regaddr + read, ptr + read, willread);
		if (rv)
			return rv;
		read += willread;
	}
	if (read < len)
		return -EIO;
	return 0;
}

int TPS25751::i2c_write(uint8_t devaddr, uint8_t regaddr, const void *data, size_t len) {
	const uint8_t *ptr = (const uint8_t*)data;
	size_t done = 0;
	while (done < len) {
		int will = len - done;
		if (will > I2C_WRITE_MAX)
			will = I2C_WRITE_MAX;
		int rv = i2c_write_single(devaddr, regaddr + done, ptr + done, will);
		if (rv)
			return rv;
		done += will;
	}
	if (done < len)
		return -EIO;
	return 0;
}

int TPS25751::i2c_read_single(uint8_t devaddr, uint8_t regaddr, uint8_t *data, size_t len) {

	if (!data)
		return -EINVAL;

	if (len > I2C_READ_MAX) {
		ESP_LOGE(TAG, "%zu - too much (%d max)", len, I2C_READ_MAX);
		return -1;
	}

	struct I2Cr_in {
		uint8_t addr:7;
		uint8_t :1;
		uint8_t regaddr;
		uint8_t len;
	} __attribute__((packed));

	struct I2Cr_out {
		uint8_t bytecount;
		uint8_t ret;
		uint8_t data[I2C_READ_MAX];
	} __attribute__((packed));

	const I2Cr_in wr = {
		.addr = devaddr,
		.regaddr = regaddr,
		.len = len,
	};
	I2Cr_out out = {};

	int rv = write_4CC("I2Cr", &wr, sizeof(wr), &out, sizeof(out));
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X read %d", devaddr, rv);
		return rv;
	}

	if (out.ret)
		return -EIO;

	memcpy(data, out.data, len);
	return 0;
}

int TPS25751::i2c_write_single(uint8_t devaddr, uint8_t regaddr, const uint8_t *data, size_t len) {

	if (!data)
		return -EINVAL;

	if (len > I2C_WRITE_MAX) {
		ESP_LOGE(TAG, "%zu - too much (%d max)", len, I2C_WRITE_MAX);
		return -1;
	}

	// structure definition is not like in docs.
	// found out experimentaly
	struct I2Cw_in {
		uint8_t addr:7;
		uint8_t :1;
		uint8_t len;
		uint8_t regaddr;
		uint8_t data[I2C_WRITE_MAX];
	} __attribute__((packed));

	struct I2Cw_out {
		uint8_t bytecount;
		uint8_t ret;
	} __attribute__((packed));

	I2Cw_in wr = {
		.addr = devaddr,
		.len = len + 1,
		.regaddr = regaddr,
	};
	I2Cw_out out = {};

	memcpy(wr.data, data, len);

	int rv = write_4CC("I2Cw", &wr, sizeof(wr), &out, sizeof(out));
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X write %d", devaddr, rv);
		return rv;
	}

	return out.ret ? -EIO : 0;
}

