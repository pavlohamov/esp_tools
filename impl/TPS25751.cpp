/*
 * TPS25751.cpp
 *
 *  Created on: 1 Aug 2026
 *      Author: pavloha
 */


#include "TPS25751.hpp"

#include <endian.h>

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
	uint32_t :15;
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
	uint32_t :76;
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


struct USB_PDO_fixed { // 00
	uint16_t maxCurrent:10; // (10 mA/unit)
	uint16_t voltage:10; // (50 mV/unit)
	uint16_t peakCurrent:2;
	uint16_t :1;
	uint16_t eprCappable:1;
	uint16_t unchunkedExt:1;
	uint16_t dualRoleData:1;
	uint16_t usbCappable:1;
	uint16_t unconstraintPower:1;
	uint16_t suspendSupported:1;
	uint16_t dualRole:1;
} __attribute__((packed));

struct USB_PDO_battery { // 01
} __attribute__((packed));

struct USB_PDO_variable { // 10
} __attribute__((packed));

struct USB_PDO_srsPps { // 11-00 APDO
} __attribute__((packed));
struct USB_PDO_eprAvs { // 11-01 APDO
} __attribute__((packed));

union USB_PDO {
	struct {
		union {
			USB_PDO_fixed fix;
			USB_PDO_battery batt;
			USB_PDO_variable var;
			USB_PDO_srsPps pps;
			USB_PDO_eprAvs avs;
		};
	};
	union {
		uint32_t raw;
		struct {
			uint32_t :30;
			uint32_t type:2;
		};
	};
} __attribute__((packed));

TPS25751::TPS25751(i2c_master_bus_handle_t bus, int addr, gpio_num_t gpio_irq):
		bus_(bus), gpio_irq_(gpio_irq), dev_(nullptr), pending_(xSemaphoreCreateBinary()), charger_(nullptr) {

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


void TPS25751::run() noexcept {


	auto writer = [&](uint8_t i2c_addr, uint8_t reg, const void *data, size_t len) {
		return i2c_write(i2c_addr, reg, data, len);
	};
	auto reader = [&](uint8_t i2c_addr, uint8_t reg, void *data, size_t len) {
		return i2c_read(i2c_addr, reg, data, len);
	};

	std::vector<uint8_t> data;

	const uint64_t start_at = esp_timer_get_time();
	bool wait = false;
	do {
		if (wait)
			vTaskDelay(pdMS_TO_TICKS(150));
		wait = true;
		int rv = readreg(3, data);
		if (rv)
			continue;

		const State st = decodeState(data.data() + 1);
		if (st == APP) {
			ESP_LOGW(TAG, "APP. Send GAID. May restart if dead battery");
#if 01
			break;
#else
			sendcmd1("GAID");
			continue;
#endif
		}

		rv = writereg(0x16, s_enableCMD1irq, sizeof(s_enableCMD1irq));
		if (rv) {
			ESP_LOGE(TAG, "ena CMD1 interrupt %d", rv);
			continue;
		}
		rv = writereg(0x18, s_clearIrq, sizeof(s_clearIrq));
		if (rv) {
			ESP_LOGE(TAG, "clear CMD1 interrupt %d", rv);
			continue;
		}

		static const PBMs_Data_Out pbdata = {
			.size = _binary_TPS25751_bin_end - _binary_TPS25751_bin_start,
			.i2c_addr = 0x30,
			.i2c_tout = 0x31,
		};
		rv = writereg(0x09, (uint8_t*)&pbdata, sizeof(pbdata));
		if (rv) {
			ESP_LOGE(TAG, "cmd data err %d", rv);
			continue;
		}

		rv = sendcmd1("PBMs");
		if (rv) {
			ESP_LOGE(TAG, "PBMs err");
			continue;
		}

		rv = readreg(0x09, data);
		if (rv) {
			ESP_LOGE(TAG, "read 9 = %d", rv);
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

		rv = sendcmd1("PBMc");
		if (rv) {
			ESP_LOGW(TAG, "PBMc err");
			continue;
		}

		rv = readreg(9, data);
		if (rv) {
			ESP_LOGE(TAG, "data read %d", rv);
			continue;
		}

//		vTaskDelay(pdMS_TO_TICKS(20));

		rv = readreg(3, data);
		if (rv) {
			ESP_LOGE(TAG, "state read %d", rv);
			continue;
		}

		if (decodeState(data.data() + 1) == APP)
			break;

	} while (running());

	// todo: check if charger present
	if (1) {
		if (charger_)
			delete charger_;
		charger_ = new BQ25798(BQ25798_ADR, writer, reader);
	}

	const uint64_t loading_took = esp_timer_get_time() - start_at;
	ESP_LOGI(TAG, "loading_took %d", (int)(loading_took / 1000UL));

	writereg(0x16, s_enableCMD1irq, sizeof(s_enableCMD1irq));
	writereg(0x18, s_clearIrq, sizeof(s_clearIrq));

	int rv = 0;
//	rv = sendcmd1("DBfg");
//	if (rv) {
//		ESP_LOGE(TAG, "DBfg err");
//	}
//
//	rv = sendcmd1("GSrC");
//	if (rv) {
//		ESP_LOGE(TAG, "GSrC err");
//	}
//
//	rv = sendcmd1("SWSk");
//	if (rv) {
//		ESP_LOGE(TAG, "SWSk err");
//	}

	xSemaphoreGive(pending_);

	if (charger_) {
		vTaskDelay(pdMS_TO_TICKS(500));
		charger_->setAdcState(true);
		charger_->setInputLimitA(2000);
		charger_->setChargeLimitA(1500);
		charger_->setInputLimitV(4000); // minimal operating voltage
		charger_->setPrechargeLimitA(160);
		charger_->setTerminationA(160);
		charger_->setPWMFrequency(BQ25798_PWM_FREQ_750KHZ);


//		charger_->setOTGV(5200);
//		charger_->setOTGLimitA(1500);
//		charger_->setBackupModeThresh(BQ25798_VBUS_BACKUP_100_PERCENT);
//		charger_->setOTGenable(true);
//		charger_->setBackupModeEnable(true);


		uint32_t sysmin_v = 0;
		uint32_t charge_v = 0;
		uint32_t charge_i = 0;
		uint32_t input_v = 0;
		uint32_t input_i = 0;
		uint32_t precharge_i = 0;
		uint32_t termcharge_i = 0;

		charger_->getMinSystemV(sysmin_v);
		charger_->getChargeLimitV(charge_v);
		charger_->getChargeLimitA(charge_i);
		charger_->getInputLimitV(input_v);
		charger_->getInputLimitA(input_i);
		charger_->getPrechargeLimitA(precharge_i);
		charger_->getTerminationA(termcharge_i);

		ESP_LOGI(TAG, "    sysmin_v  %6d mV", sysmin_v);
		ESP_LOGI(TAG, "    charge_v  %6d mV", charge_v);
		ESP_LOGI(TAG, "    charge_i  %6d mA", charge_i);
		ESP_LOGI(TAG, "     input_v  %6d mV", input_v);
		ESP_LOGI(TAG, "     input_i  %6d mA", input_i);
		ESP_LOGI(TAG, " precharge_i  %6d mA", precharge_i);
		ESP_LOGI(TAG, "      term_i  %6d mA", termcharge_i);


		uint32_t otg_mv;
		uint32_t otg_ma;

		charger_->getOTGV(otg_mv);
		charger_->getOTGLimitA(otg_ma);

		bool en = 0;
		charger_->getBackupModeEnable(en);
		ESP_LOGI(TAG, "   getBackupModeEnable %d otg v %d a %d", en, otg_mv, otg_ma);

//		rv = sendcmd1("DBfg");
//		if (rv) {
//			ESP_LOGE(TAG, "DBfg err");
//		}
	}

	while (running()) {
		const bool taken = xSemaphoreTake(pending_, pdMS_TO_TICKS(500));
		if (taken) {
			readreg(0x14, data);
			writereg(0x18, s_clearIrq, sizeof(s_clearIrq));
			ESP_LOGW(TAG, "isr");
			ESP_LOG_BUFFER_HEXDUMP(TAG, data.data() + 1, data.size() - 1, ESP_LOG_INFO);
		}

		if (charger_) {
			BQ25798::AdcResult adc;
			uint16_t faults = 0;
			uint8_t status[5];
			if (charger_->getAdcResults(adc)) {
				ESP_LOGI(TAG, "BATT: %6dmV %6dmA", adc.vbat_mv, adc.ibatt_ma);
				ESP_LOGI(TAG, " BUS: %6dmV %6dmA", adc.vbus_mv, adc.ibus_ma);
				ESP_LOGI(TAG, " SYS: %6dmV 1 %6d 2 %6d", adc.vsys_mv, adc.vac1_mv, adc.vac2_mv);
				ESP_LOGI(TAG, "   T: %3d %3d", adc.ts, adc.tdie);
			}
			if (charger_->getFaults(faults))
				ESP_LOGI(TAG, "faults: 0x%4X", faults);
			if (charger_->getStatus(status))
				ESP_LOG_BUFFER_HEXDUMP(TAG, status, sizeof(status), ESP_LOG_INFO);

			bool en = 0;
			if (charger_->getBackupModeEnable(en) && !en) {
				ESP_LOGI(TAG, "   getBackupModeEnable %d", en);
				charger_->setBackupModeEnable(true);
			}
		}
//		dump();
//		for (size_t i = 7; i < ARRAY_SIZE(s_regmap); ++i) {
//			int rv = readreg(s_regmap[i].addr, data);
//			if (rv) {
//				ESP_LOGE(TAG, "%d read 0x%X = %d", i, s_regmap[i].addr, rv);
//				continue;
//			}
//
//			ESP_LOGW(TAG, "0x%X", s_regmap[i].addr);
//			ESP_LOG_BUFFER_HEXDUMP(TAG, data.data() + 1, data.size() - 1, ESP_LOG_INFO);
//		}
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


int TPS25751::sendcmd1(const char* data) noexcept {

	xSemaphoreTake(pending_, 0);
	int rv = writereg(0x08, (const uint8_t*)data, 4);
	if (rv) {
		ESP_LOGE(TAG, "cmd1 %.4s write %d", data, rv);
		return rv;
	}

	if (xSemaphoreTake(pending_, pdMS_TO_TICKS(150))) {
		rv = writereg(0x18, s_clearIrq, sizeof(s_clearIrq));
		if (rv) {
			ESP_LOGE(TAG, "cmd1 %.4s clear irq %d", data, rv);
			return rv;
		}
	} else {
		ESP_LOGE(TAG, "cmd1 %.4s timeout", data);
		return -ETIMEDOUT;
	}

	uint8_t ack[5];
	rv = readreg(0x08, ack, sizeof(ack));
	if (rv) {
		ESP_LOGE(TAG, "cmd1 %.4s ack read %d", data, rv);
		return rv;
	}
	if (ack[0] != 4) {
		ESP_LOGE(TAG, "cmd1 %.4s ack invalid len %d", data, ack[0]);
		return -1;
	}

	const uint8_t value = *(uint32_t*)(ack + 1);
	if (value) {
		ESP_LOGE(TAG, "cmd1 %.4s ack 0x%X", data, value);
		return -1;
	}
	return 0;
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

	i2c_master_bus_rm_device(write_dev);

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

	int rv = writereg(0x09, (const uint8_t*)&wr, sizeof(wr));
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X read %d", devaddr, rv);
		return rv;
	}

	rv = sendcmd1("I2Cr");
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X read %d", devaddr, rv);
		return rv;
	}

	I2Cr_out out = {};
	rv = readreg(0x09, (uint8_t*)&out, sizeof(out));
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

	memcpy(wr.data, data, len);

	int rv = writereg(0x09, (const uint8_t*)&wr, sizeof(wr));
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X write %d", devaddr, rv);
		return rv;
	}

	rv = sendcmd1("I2Cw");
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X write %d", devaddr, rv);
		return rv;
	}

	I2Cw_out out = {};
	rv = readreg(0x09, (uint8_t*)&out, sizeof(out));
	if (rv) {
		ESP_LOGE(TAG, "i2c 0x%02X read %d", devaddr, rv);
		return rv;
	}

	return out.ret ? -EIO : 0;
}


void TPS25751::dump() {
	dumpStatus();
	dumpPowerPath();
	dumpPdo(0x30, "Rx SRC");
//	dumpPdo(0x31, "Rx Sink");
//	dumpPdo(0x32, "Tx SRC", 2);
//	dumpPdo(0x33, "Tx Sink");

//	std::vector<uint8_t> data;
//	if (!readreg(0x37, data)) {
//		const AutonegotiateSink *as = (AutonegotiateSink*)(data.data() + 1);
//		ESP_LOGW(TAG, " autoPrioLowVolt %d", as->autoPrioLowVolt);
//		ESP_LOGW(TAG, " noUsbSuspend %d", as->autoComputeSinkPower);
//		ESP_LOGW(TAG, " autoComputeSinkPower %d", as->autoPrioLowVolt);
//		ESP_LOGW(TAG, " noCapMissmatch %d", as->noCapMissmatch);
//		ESP_LOGW(TAG, " autoComputeSinkMinVolt %d", as->autoComputeSinkMinVolt);
//		ESP_LOGW(TAG, " autoComputeSinkMaxVolt %d", as->autoComputeSinkMaxVolt);
//		ESP_LOGW(TAG, " autoDisableSink %d", as->autoDisableSink);
//		ESP_LOGW(TAG, " autoNegSinkMinPower %d", as->autoNegSinkMinPower * 250);
//		ESP_LOGW(TAG, " autoNegMaxVolt %d", as->autoNegMaxVolt * 50);
//		ESP_LOGW(TAG, " autoNegMinVolt %d", as->autoNegMinVolt * 50);
//		ESP_LOGW(TAG, " autoNegMissmatchPower %d", as->autoNegMissmatchPower * 250);
//		ESP_LOGW(TAG, " ppsEnaSink %d", as->ppsEnaSink);
//		ESP_LOGW(TAG, " ppsReqInterval %d", as->ppsReqInterval);
//		ESP_LOGW(TAG, " ppsSourceMode %d", as->ppsSourceMode);
//		ESP_LOGW(TAG, " ppsReqFullVolt %d", as->ppsReqFullVolt);
//		ESP_LOGW(TAG, " ppsDisSinkNotAPDO %d", as->ppsDisSinkNotAPDO);
//		ESP_LOGW(TAG, " ppsOpCurrent %d", as->ppsOpCurrent * 50);
//		ESP_LOGW(TAG, " ppsOpVoltage %d", as->ppsOpVoltage * 20);
//	}


}

void TPS25751::dumpStatus() {

	char textic[256];
	std::vector<uint8_t> data;
	readreg(0x1A, data);
	const Status_Out* stt = (Status_Out*)(data.data() + 1);

	int occ = snprintf(textic, sizeof(textic), "%s", stt->plugPresent ? "Plugged" : "empty");
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\nConn %d", stt->connectionState);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n%s", stt->plugOrientation ? "UD" : "UU");
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n%s", stt->portRole ? "source" : "sink");
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n%s", stt->dataRole ? "DFP" : "UFP");
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\nvbus %d", stt->vbusStatus);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\nusbHost %d", stt->usbHostPresent);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\nlegacy %d", stt->actingLegacy);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\nbist %d", stt->bist);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\ntout %d", stt->socAckTout);

	ESP_LOGI(TAG, "Status_Out %d %s", occ, textic);
}

void TPS25751::dumpPowerPath() {

	char textic[256];
	std::vector<uint8_t> data;
	readreg(0x26, data);
	const PowerPathStatus_Out* stt = (PowerPathStatus_Out*)(data.data() + 1);

	int occ = snprintf(textic, sizeof(textic), "ppCable1 %d", stt->ppCable1);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n ppSwitch1 %d", stt->ppSwitch1);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n ppSwitch3 %d", stt->ppSwitch3);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n pp1overcurrent %d %d", stt->pp1overcurrent, stt->ppCable1overcurrent);
	occ += snprintf(textic + occ, sizeof(textic) - occ, "\n powerSource %d", stt->powerSource);

	ESP_LOGI(TAG, "power %d %s", occ, textic);
}

void TPS25751::dumpPdo(int addr, const char* text, int extraOffset) {

	std::vector<uint8_t> data;
	readreg(addr, data);
	const size_t count = data.data()[1];
	const USB_PDO* stt = (USB_PDO*)(data.data() + 2 + extraOffset);

	if (sizeof(USB_PDO) != 4) {
		ESP_LOGE(TAG, "aaa %d %d", sizeof(USB_PDO), sizeof(USB_PDO_fixed));
		abort();
	}

	if (count > 7) {
		ESP_LOGE(TAG, "unsupported pdos %zu", count);
		ESP_LOG_BUFFER_HEXDUMP(TAG, data.data() + 1, data.size() - 1, ESP_LOG_INFO);
		return;
	}

	for (size_t i = 0; i < count; ++i) {
		char textic[512];
		int occ = snprintf(textic, sizeof(textic), "%d: 0x%8X type %d", i, stt[i].raw, stt[i].type);
		if (!stt[i].type) {
			occ += snprintf(textic + occ, sizeof(textic) - occ, "\n dual %d", stt[i].fix.dualRole);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " data %d", stt[i].fix.dualRoleData);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " susp %d", stt[i].fix.suspendSupported);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " unco %d", stt[i].fix.unconstraintPower);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " usb %d", stt[i].fix.usbCappable);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " uck %d", stt[i].fix.unchunkedExt);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " epr %d", stt[i].fix.eprCappable);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " peak %d", stt[i].fix.peakCurrent);
			occ += snprintf(textic + occ, sizeof(textic) - occ, "\n %5dmV", stt[i].fix.voltage * 50);
			occ += snprintf(textic + occ, sizeof(textic) - occ, " %5dmA", stt[i].fix.maxCurrent * 10);
		} else {
//			occ += snprintf(textic + occ, sizeof(textic) - occ, "\n dual %d", stt[i].fix.dualRole);
		}
		if (text)
			ESP_LOGI(TAG, "PDO %s %s", text, textic);
		else
			ESP_LOGI(TAG, "PDO 0x%X %s", addr, textic);
	}
}
