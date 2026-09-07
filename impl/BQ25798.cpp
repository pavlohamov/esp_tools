/*
 * BQ25798.cpp
 *
 *  Created on: 11 Aug 2026
 *      Author: pavloha
 */



#include "BQ25798.hpp"

#include <endian.h>


#include "esp_log.h"
static const char *TAG = "BQ25798";


BQ25798::BQ25798(int addr, i2c_writer writer, i2c_reader reader) noexcept: addr_(addr), write_(writer), read_(reader) {

}

BQ25798::~BQ25798() noexcept {

}

bool BQ25798::getBit(int regaddr, int bitoffset, bool &bv, const char *calle) {
	uint8_t val = 0;
	int rv = read_(addr_, regaddr, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", calle, rv);
		return false;
	}
	bv = !!(val & (1 << bitoffset));
	return true;

}
bool BQ25798::setBit(int regaddr, int bitoffset, bool bv, const char *calle) {
	uint8_t val = 0;
	int rv = read_(addr_, regaddr, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", calle, rv);
		return false;
	}
	val = (val & ~(1 << bitoffset)) | (bv << bitoffset);
	return !write_(addr_, regaddr, &val, 1);
}

int BQ25798::readReg(uint8_t regaddr) {
	uint8_t val = 0;
	int rv = read_(addr_, regaddr, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "failed %d", rv);
		return rv > 0 ? -rv : rv;
	}
	return val;
}


bool BQ25798::getAdcState(bool& on) {
	return getBit(BQ25798_REG_ADC_CONTROL, 7, on, __FUNCTION__);
}
bool BQ25798::setAdcState(bool on) {
	uint8_t val = on ? 0x88 : 0;
	return !write_(addr_, BQ25798_REG_ADC_CONTROL, &val, 1);
}

bool BQ25798::getAdcResults(AdcResult &res) {
	uint8_t buff[BQ25798_REG_DPLUS_ADC - BQ25798_REG_IBUS_ADC];
	int rv = read_(addr_, BQ25798_REG_IBUS_ADC, buff, sizeof(buff));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	const uint16_t* ptr = (const uint16_t*)buff;
	res.ibus_ma = (int16_t)htobe16(ptr[0]);
	res.ibatt_ma = (int16_t)htobe16(ptr[1]);
	res.vbus_mv = (uint16_t)htobe16(ptr[2]);
	res.vac1_mv = (uint16_t)htobe16(ptr[3]);
	res.vac2_mv = (uint16_t)htobe16(ptr[4]);
	res.vbat_mv = (uint16_t)htobe16(ptr[5]);
	res.vsys_mv = (uint16_t)htobe16(ptr[6]);
	res.ts = (uint16_t)htobe16(ptr[7]);
	res.tdie = (uint16_t)htobe16(ptr[8]);
	return true;
}

bool BQ25798::getFaults(uint16_t& faults) {
	int rv = read_(addr_, BQ25798_REG_FAULT_STATUS_0, &faults, sizeof(faults));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	faults = htobe16(faults);
	return true;
}

bool BQ25798::getStatus(uint8_t status[5]) {
	int rv = read_(addr_, BQ25798_REG_CHARGER_STATUS_0, status, 5);
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	return true;
}


bool BQ25798::getMinSystemV(uint32_t& mv) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MINIMAL_SYSTEM_VOLTAGE, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mv = (val & 0x3F) * 250 + 2500;
	return true;
}
bool BQ25798::setMinSystemV(uint32_t mv) {
	mv = (mv - 2500) / 250;
	if (mv > 0x3F) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, mv);
		return false;
	}
	return !write_(addr_, BQ25798_REG_MINIMAL_SYSTEM_VOLTAGE, &mv, 1);
}

bool BQ25798::getChargeLimitV(uint32_t& mv) {
	uint16_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGE_VOLTAGE_LIMIT, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mv = (htobe16(val) & 0x7FF) * 10;
	return true;
}
bool BQ25798::setChargeLimitV(uint32_t mv) {
	mv /= 10;
	if (mv > 0x7FF) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, mv);
		return false;
	}
	mv = htobe16(mv);
	return !write_(addr_, BQ25798_REG_CHARGE_VOLTAGE_LIMIT, &mv, 2);
}

bool BQ25798::getChargeLimitA(uint32_t& ma) {
	uint16_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGE_CURRENT_LIMIT, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	ma = (htobe16(val) & 0x1FF) * 10;
	return true;
}
bool BQ25798::setChargeLimitA(uint32_t ma) {
	if (ma > 5000 || ma < 50) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, ma);
		return false;
	}
	ma /= 10;
	ma = htobe16(ma);
	return !write_(addr_, BQ25798_REG_CHARGE_CURRENT_LIMIT, &ma, 2);
}

bool BQ25798::getInputLimitV(uint32_t& mv) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_INPUT_VOLTAGE_LIMIT, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mv = val * 100;
	return true;
}
bool BQ25798::setInputLimitV(uint32_t mv) {
	mv /= 100;
	if (mv > 0xFF) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, mv);
		return false;
	}
	return !write_(addr_, BQ25798_REG_INPUT_VOLTAGE_LIMIT, &mv, 1);
}

bool BQ25798::getInputLimitA(uint32_t& ma) {
	uint16_t val = 0;
	int rv = read_(addr_, BQ25798_REG_INPUT_CURRENT_LIMIT, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	ma = (htobe16(val) & 0x1FF) * 10;
	return true;
}
bool BQ25798::setInputLimitA(uint32_t ma) {
	if (ma > 3300) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, ma);
		return false;
	}
	ma /= 10;
	ma = htobe16(ma);
	return !write_(addr_, BQ25798_REG_INPUT_CURRENT_LIMIT, &ma, 2);
}

bool BQ25798::getVBatLowV(bq25798_vbat_lowv_t &thr) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	thr = bq25798_vbat_lowv_t(val >> 6);
	return true;

}
bool BQ25798::setVBatLowV(bq25798_vbat_lowv_t thr) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x3F) | (thr << 6);
	return !write_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, 1);
}

bool BQ25798::getPrechargeLimitA(uint32_t& ma) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	ma = (val & 0x3F) * 40;
	return true;
}
bool BQ25798::setPrechargeLimitA(uint32_t ma) {

	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = ((ma / 40) & 0x3F) | (val << 6);
	return !write_(addr_, BQ25798_REG_PRECHARGE_CONTROL, &val, 1);
}

bool BQ25798::getStopOnWDT(bool& stop) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	stop = !!(val & (1 << 5));
	return true;

}
bool BQ25798::setStopOnWDT(bool stop) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val &= ~(1 << 5);
	if (stop)
		val |= (1 << 5);
	return !write_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, 1);
}

bool BQ25798::getTerminationA(uint32_t& ma) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	ma = (val & 0x1F) * 40;
	return true;
}
bool BQ25798::setTerminationA(uint32_t ma) {
	ma /= 40;
	if (ma > 0x1F) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, ma);
		return false;
	}
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xE0) | ma;
	return !write_(addr_, BQ25798_REG_TERMINATION_CONTROL, &val, 1);
}

bool BQ25798::getCellCount(bq25798_cell_count_t &count) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	count = (bq25798_cell_count_t)((val >> 6) & 0x3);
	return true;
}
bool BQ25798::setCellCount(bq25798_cell_count_t count) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x3F) | (count << 6);
	return !write_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, 1);
}

bool BQ25798::getRechargeDeglitchTime(bq25798_trechg_time_t &deglitchTime) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	deglitchTime = (bq25798_trechg_time_t)((val >> 4) & 0x3);
	return true;
}
bool BQ25798::setRechargeDeglitchTime(bq25798_trechg_time_t deglitchTime) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xCF) | (deglitchTime << 4);
	return !write_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, 1);
}

bool BQ25798::getRechargeThreshOffsetV(uint32_t& mv) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mv = (val & 0xF) * 50 + 50;
	return true;
}
bool BQ25798::setRechargeThreshOffsetV(uint32_t mv) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xF0) | ((mv - 50) / 50);
	return !write_(addr_, BQ25798_REG_RECHARGE_CONTROL, &val, 1);
}

bool BQ25798::getOTGV(uint32_t& mv) {
	uint16_t val = 0;
	int rv = read_(addr_, BQ25798_REG_VOTG_REGULATION, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mv = (htobe16(val) & 0x7FF) * 10 + 2800;
	return true;
}
bool BQ25798::setOTGV(uint32_t mv) {
	if (mv > 22000) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, mv);
		return false;
	}
	mv = htobe16((mv - 2800) / 10);
	return !write_(addr_, BQ25798_REG_VOTG_REGULATION, &mv, 2);
}

bool BQ25798::getPrechargeTimer(bq25798_prechg_timer_t& timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_IOTG_REGULATION, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	timer = (val & 0x80) ? BQ25798_PRECHG_TMR_0_5HR : BQ25798_PRECHG_TMR_2HR;
	return true;

}
bool BQ25798::setPrechargeTimer(bq25798_prechg_timer_t timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_IOTG_REGULATION, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x7F) | (timer << 7);
	return !write_(addr_, BQ25798_REG_IOTG_REGULATION, &val, 1);
}

bool BQ25798::getOTGLimitA(uint32_t& ma) {

	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_IOTG_REGULATION, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	ma = (val & 0x7F) * 40;
	return true;
}
bool BQ25798::setOTGLimitA(uint32_t ma) {
	if (ma > 3360 || ma < 160) {
		ESP_LOGE(TAG, "%s invalid %d", __FUNCTION__, ma);
		return false;
	}

	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_IOTG_REGULATION, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x80) | (ma / 40);
	return !write_(addr_, BQ25798_REG_IOTG_REGULATION, &val, 1);
}

bool BQ25798::getTopOffTimer(bq25798_topoff_timer_t& timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TIMER_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	timer = bq25798_topoff_timer_t(val >> 6);
	return true;
}
bool BQ25798::setTopOffTimer(bq25798_topoff_timer_t timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TIMER_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x3F) | (timer << 6);
	return !write_(addr_, BQ25798_REG_TIMER_CONTROL, &val, 1);
}

bool BQ25798::getTrickleChargeTimerEnable(bool& enable) {
	return getBit(BQ25798_REG_TIMER_CONTROL, 5, enable, __FUNCTION__);
}
bool BQ25798::setTrickleChargeTimerEnable(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 5, enable, __FUNCTION__);
}

bool BQ25798::getPrechargeTimerEnable(bool& enable) {
	return getBit(BQ25798_REG_TIMER_CONTROL, 4, enable, __FUNCTION__);
}
bool BQ25798::setPrechargeTimerEnable(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 4, enable, __FUNCTION__);
}

bool BQ25798::getFastChargeTimerEnable(bool& enable) {
	return getBit(BQ25798_REG_TIMER_CONTROL, 3, enable, __FUNCTION__);
}
bool BQ25798::setFastChargeTimerEnable(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 3, enable, __FUNCTION__);
}

bool BQ25798::getFastChargeTimer(bq25798_chg_timer_t& timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TIMER_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	timer = bq25798_chg_timer_t((val >> 1) & 0x3);
	return true;
}
bool BQ25798::setFastChargeTimer(bq25798_chg_timer_t timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TIMER_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xF9) | (timer << 1);
	return !write_(addr_, BQ25798_REG_TIMER_CONTROL, &val, 1);
}

bool BQ25798::getTimerHalfRateEnable(bool& enable) {
	return getBit(BQ25798_REG_TIMER_CONTROL, 0, enable, __FUNCTION__);
}
bool BQ25798::setTimerHalfRateEnable(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 0, enable, __FUNCTION__);
}

bool BQ25798::getAutoOVPBattDischarge(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 7, enable, __FUNCTION__);
}
bool BQ25798::setAutoOVPBattDischarge(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 7, enable, __FUNCTION__);
}

bool BQ25798::getForceBattDischarge(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 6, enable, __FUNCTION__);
}
bool BQ25798::setForceBattDischarge(bool enable) {
	return setBit(BQ25798_REG_TIMER_CONTROL, 6, enable, __FUNCTION__);
}

bool BQ25798::getChargeEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 5, enable, __FUNCTION__);
}
bool BQ25798::setChargeEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 5, enable, __FUNCTION__);
}

bool BQ25798::getICOEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 4, enable, __FUNCTION__);
}
bool BQ25798::setICOEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 4, enable, __FUNCTION__);
}

bool BQ25798::getForceICO(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 3, enable, __FUNCTION__);
}
bool BQ25798::setForceICO(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 3, enable, __FUNCTION__);
}

bool BQ25798::getHIZMode(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 2, enable, __FUNCTION__);
}
bool BQ25798::setHIZMode(bool enable){
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 2, enable, __FUNCTION__);
}

bool BQ25798::getTerminationEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 1, enable, __FUNCTION__);
}
bool BQ25798::setTerminationEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 1, enable, __FUNCTION__);
}

bool BQ25798::getBackupModeEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_0, 0, enable, __FUNCTION__);
}
bool BQ25798::setBackupModeEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_0, 0, enable, __FUNCTION__);
}

bool BQ25798::getBackupModeThresh(bq25798_vbus_backup_t& threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	threshold = bq25798_vbus_backup_t((val >> 6) & 0x3);
	return true;
}
bool BQ25798::setBackupModeThresh(bq25798_vbus_backup_t threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x3F) | (threshold << 6);
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, 1);
}

bool BQ25798::getVACOVP(bq25798_vac_ovp_t& threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	threshold = bq25798_vac_ovp_t((val >> 4) & 0x3);
	return true;
}
bool BQ25798::setVACOVP(bq25798_vac_ovp_t threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xCF) | (threshold << 4);
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, 1);
}

bool BQ25798::resetWDT() {
	return setBit(BQ25798_REG_CHARGER_CONTROL_1, 3, 1, __FUNCTION__);
}

bool BQ25798::getWDT(bq25798_wdt_t& timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	timer = bq25798_wdt_t(val & 0x7);
	return true;
}
bool BQ25798::setWDT(bq25798_wdt_t timer) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xF8) | timer;
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_1, &val, 1);
}

bool BQ25798::getForceDPinsDetection(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 7, enable, __FUNCTION__);
}
bool BQ25798::setForceDPinsDetection(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 7, enable, __FUNCTION__);
}

bool BQ25798::getAutoDPinsDetection(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 6, enable, __FUNCTION__);
}
bool BQ25798::setAutoDPinsDetection(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 6, enable, __FUNCTION__);
}

bool BQ25798::getHVDCP12VEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 5, enable, __FUNCTION__);
}
bool BQ25798::setHVDCP12VEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 5, enable, __FUNCTION__);
}

bool BQ25798::getHVDCP9VEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 4, enable, __FUNCTION__);
}
bool BQ25798::setHVDCP9VEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 4, enable, __FUNCTION__);
}

bool BQ25798::getHVDCPEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 3, enable, __FUNCTION__);
}
bool BQ25798::setHVDCPEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 3, enable, __FUNCTION__);
}

bool BQ25798::getShipFETmode(bq25798_sdrv_ctrl_t& mode) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_2, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	mode = bq25798_sdrv_ctrl_t((val >> 1) & 0x3);
	return true;
}
bool BQ25798::setShipFETmode(bq25798_sdrv_ctrl_t mode) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_2, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xF9) | (mode << 1);
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_2, &val, 1);
}

bool BQ25798::getShipFET10sDelay(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_2, 0, enable, __FUNCTION__);
}
bool BQ25798::setShipFET10sDelay(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_2, 7, enable, __FUNCTION__);
}

bool BQ25798::getACenable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 7, enable, __FUNCTION__);
}
bool BQ25798::setACenable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 7, enable, __FUNCTION__);
}

bool BQ25798::getOTGenable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 6, enable, __FUNCTION__);
}
bool BQ25798::setOTGenable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 6, enable, __FUNCTION__);
}

bool BQ25798::getOTGPFM(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 5, enable, __FUNCTION__);
}
bool BQ25798::setOTGPFM(bool enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 5, enable, __FUNCTION__);
}

bool BQ25798::getForwardPFM(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 4, enable, __FUNCTION__);
}
bool BQ25798::setForwardPFM(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 4, enable, __FUNCTION__);
}

bool BQ25798::getShipWakeupDelay(bq25798_wkup_dly_t& delay) {
	bool val = 0;
	if (!getBit(BQ25798_REG_CHARGER_CONTROL_3, 3, val, __FUNCTION__))
		return false;
	delay = bq25798_wkup_dly_t(val);
	return true;
}
bool BQ25798::setShipWakeupDelay(bq25798_wkup_dly_t delay) {
	bool val = delay == BQ25798_WKUP_DLY_15MS;
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 3, val, __FUNCTION__);
}

bool BQ25798::getBATFETLDOprecharge(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 2, enable, __FUNCTION__);
}
bool BQ25798::setBATFETLDOprecharge(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 2, enable, __FUNCTION__);
}

bool BQ25798::getOTGOOA(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 1, enable, __FUNCTION__);
}
bool BQ25798::setOTGOOA(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_3, 1, enable, __FUNCTION__);
}

bool BQ25798::getForwardOOA(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 0, enable, __FUNCTION__);
}
bool BQ25798::setForwardOOA(bool enable)  {
	return getBit(BQ25798_REG_CHARGER_CONTROL_3, 0, enable, __FUNCTION__);
}

bool BQ25798::getACDRV2enable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 7, enable, __FUNCTION__);
}
bool BQ25798::setACDRV2enable(bool enable)  {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 7, enable, __FUNCTION__);
}

bool BQ25798::getACDRV1enable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 6, enable, __FUNCTION__);
}
bool BQ25798::setACDRV1enable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 6, enable, __FUNCTION__);
}

bool BQ25798::getPWMFrequency(bq25798_pwm_freq_t& frequency) {
	bool val = 0;
	if (!getBit(BQ25798_REG_CHARGER_CONTROL_4, 5, val, __FUNCTION__))
		return false;
	frequency = bq25798_pwm_freq_t(val);
	return true;
}
bool BQ25798::setPWMFrequency(bq25798_pwm_freq_t frequency) {
	bool val = frequency == BQ25798_PWM_FREQ_750KHZ;
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 5, val, __FUNCTION__);
}

bool BQ25798::getStatPinEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 4, enable, __FUNCTION__);
}
bool BQ25798::setStatPinEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 4, enable, __FUNCTION__);
}

bool BQ25798::getVSYSshortProtect(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 3, enable, __FUNCTION__);
}
bool BQ25798::setVSYSshortProtect(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 3, enable, __FUNCTION__);
}

bool BQ25798::getVOTG_UVPProtect(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 2, enable, __FUNCTION__);
}
bool BQ25798::setVOTG_UVPProtect(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 2, enable, __FUNCTION__);
}

bool BQ25798::getIBUS_OCPenable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 1, enable, __FUNCTION__);
}
bool BQ25798::setIBUS_OCPenable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 1, enable, __FUNCTION__);
}

bool BQ25798::getVINDPMdetection(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_4, 0, enable, __FUNCTION__);
}
bool BQ25798::setVINDPMdetection(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_4, 0, enable, __FUNCTION__);
}

bool BQ25798::getShipFETpresent(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 7, enable, __FUNCTION__);
}
bool BQ25798::setShipFETpresent(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_5, 7, enable, __FUNCTION__);
}

bool BQ25798::getBatDischargeSenseEnable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 6, enable, __FUNCTION__);
}
bool BQ25798::setBatDischargeSenseEnable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_5, 6, enable, __FUNCTION__);
}

bool BQ25798::getBatDischargeA(bq25798_ibat_reg_t& current) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_5, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	current = bq25798_ibat_reg_t((val >> 3) & 0x3);
	return true;
}
bool BQ25798::setBatDischargeA(bq25798_ibat_reg_t current) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_5, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xE7) | (current << 3);
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_5, &val, 1);
}

bool BQ25798::getIINDPMenable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 2, enable, __FUNCTION__);
}
bool BQ25798::setIINDPMenable(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_5, 2, enable, __FUNCTION__);
}

bool BQ25798::getExtILIMpin(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 1, enable, __FUNCTION__);
}
bool BQ25798::setExtILIMpin(bool enable) {
	return setBit(BQ25798_REG_CHARGER_CONTROL_5, 1, enable, __FUNCTION__);
}

bool BQ25798::getBatDischargeOCPenable(bool& enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 0, enable, __FUNCTION__);
}
bool BQ25798::setBatDischargeOCPenable(bool enable) {
	return getBit(BQ25798_REG_CHARGER_CONTROL_5, 0, enable, __FUNCTION__);
}

bool BQ25798::getVINDPM_VOCpercent(bq25798_voc_pct_t& percentage) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MPPT_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	percentage = bq25798_voc_pct_t((val >> 5) & 0x7);
	return true;
}
bool BQ25798::setVINDPM_VOCpercent(bq25798_voc_pct_t percentage) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_CHARGER_CONTROL_5, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x1F) | (percentage << 5);
	return !write_(addr_, BQ25798_REG_CHARGER_CONTROL_5, &val, 1);
}

bool BQ25798::getVOCdelay(bq25798_voc_dly_t& delay) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MPPT_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	delay = bq25798_voc_dly_t((val >> 3) & 0x3);
	return true;
}
bool BQ25798::setVOCdelay(bq25798_voc_dly_t delay) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MPPT_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xE7) | (delay << 3);
	return !write_(addr_, BQ25798_REG_MPPT_CONTROL, &val, 1);
}

bool BQ25798::getVOCrate(bq25798_voc_rate_t& rate) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MPPT_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	rate = bq25798_voc_rate_t((val >> 1) & 0x3);
	return true;
}
bool BQ25798::setVOCrate(bq25798_voc_rate_t rate) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_MPPT_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xF9) | (rate << 1);
	return !write_(addr_, BQ25798_REG_MPPT_CONTROL, &val, 1);

}

bool BQ25798::getMPPTenable(bool& enable) {
	return getBit(BQ25798_REG_MPPT_CONTROL, 0, enable, __FUNCTION__);
}
bool BQ25798::setMPPTenable(bool enable) {
	return setBit(BQ25798_REG_MPPT_CONTROL, 0, enable, __FUNCTION__);
}

bool BQ25798::getThermRegulationThresh(bq25798_treg_t& threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	threshold = bq25798_treg_t((val >> 6) & 0x3);
	return true;
}
bool BQ25798::setThermRegulationThresh(bq25798_treg_t threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0x3F) | (threshold << 6);
	return !write_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, 1);
}

bool BQ25798::getThermShutdownThresh(bq25798_tshut_t& threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	threshold = bq25798_tshut_t((val >> 4) & 0x3);
	return true;
}
bool BQ25798::setThermShutdownThresh(bq25798_tshut_t threshold) {
	uint8_t val = 0;
	int rv = read_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, sizeof(val));
	if (rv) {
		ESP_LOGE(TAG, "%s failed %d", __FUNCTION__, rv);
		return false;
	}
	val = (val & 0xCF) | (threshold << 4);
	return !write_(addr_, BQ25798_REG_TEMPERATURE_CONTROL, &val, 1);
}

bool BQ25798::getVBUSpulldown(bool& enable) {
	return getBit(BQ25798_REG_TEMPERATURE_CONTROL, 3, enable, __FUNCTION__);
}
bool BQ25798::setVBUSpulldown(bool enable) {
	return setBit(BQ25798_REG_TEMPERATURE_CONTROL, 3, enable, __FUNCTION__);
}

bool BQ25798::getVAC1pulldown(bool& enable) {
	return getBit(BQ25798_REG_TEMPERATURE_CONTROL, 2, enable, __FUNCTION__);
}
bool BQ25798::setVAC1pulldown(bool enable) {
	return setBit(BQ25798_REG_TEMPERATURE_CONTROL, 2, enable, __FUNCTION__);
}

bool BQ25798::getVAC2pulldown(bool& enable) {
	return getBit(BQ25798_REG_TEMPERATURE_CONTROL, 1, enable, __FUNCTION__);
}
bool BQ25798::setVAC2pulldown(bool enable) {
	return setBit(BQ25798_REG_TEMPERATURE_CONTROL, 1, enable, __FUNCTION__);
}

bool BQ25798::getBackupACFET1on(bool& enable) {
	return getBit(BQ25798_REG_TEMPERATURE_CONTROL, 0, enable, __FUNCTION__);
}
bool BQ25798::setBackupACFET1on(bool enable) {
	return setBit(BQ25798_REG_TEMPERATURE_CONTROL, 0, enable, __FUNCTION__);
}

bool BQ25798::reset() {
	return setBit(BQ25798_REG_TERMINATION_CONTROL, 6, 1, __FUNCTION__);
}

