/*
 * BQ25798.hpp
 *
 *  Created on: 11 Aug 2026
 *      Author: pavloha
 */

#pragma once

#include "Runnable.hpp"

#include "BQ25798_regs.hpp"

#include <functional>
#include <stddef.h>
#include <stdint.h>


class BQ25798: public NonCopyable, public NonMoveable {
public:
	using i2c_writer = std::function<int(uint8_t i2c_addr, uint8_t reg, const void *data, size_t len)>;
	using i2c_reader = std::function<int(uint8_t i2c_addr, uint8_t reg, void *data, size_t len)>;

	BQ25798(int addr, i2c_writer writer, i2c_reader reader) noexcept;
	virtual ~BQ25798() noexcept;

	struct AdcResult {
		int16_t ibus_ma;
		int16_t ibatt_ma;
		uint16_t vbus_mv;
		uint16_t vac1_mv;
		uint16_t vac2_mv;
		uint16_t vbat_mv;
		uint16_t vsys_mv;
		uint16_t ts;
		int16_t tdie; // in 0.5 steps
	};

private:
	const int addr_;
	const i2c_writer write_;
	const i2c_reader read_;

	bool getBit(int regaddr, int bitoffset, bool &val, const char *calle);
	bool setBit(int regaddr, int bitoffset, bool val, const char *calle);

public:
	bool getAdcState(bool& on);
	bool setAdcState(bool on);

	bool getAdcResults(AdcResult &res);

	bool getFaults(uint16_t& faults);
	bool getStatus(uint8_t status[5]);

	bool getMinSystemV(uint32_t& mv);
	bool setMinSystemV(uint32_t mv);

	bool getChargeLimitV(uint32_t& mv);
	bool setChargeLimitV(uint32_t mv);

	bool getChargeLimitA(uint32_t& ma);
	bool setChargeLimitA(uint32_t ma);

	bool getInputLimitV(uint32_t& mv);
	bool setInputLimitV(uint32_t mv);

	bool getInputLimitA(uint32_t& ma);
	bool setInputLimitA(uint32_t ma);

	bool getVBatLowV(bq25798_vbat_lowv_t &thr);
	bool setVBatLowV(bq25798_vbat_lowv_t thr);

	bool getPrechargeLimitA(uint32_t& ma);
	bool setPrechargeLimitA(uint32_t ma);

	bool getStopOnWDT(bool& stopOnWDT);
	bool setStopOnWDT(bool stopOnWDT);

	bool getTerminationA(uint32_t& ma);
	bool setTerminationA(uint32_t ma);

	bool getCellCount(bq25798_cell_count_t &count);
	bool setCellCount(bq25798_cell_count_t count);

	bool getRechargeDeglitchTime(bq25798_trechg_time_t &deglitchTime);
	bool setRechargeDeglitchTime(bq25798_trechg_time_t deglitchTime);

	bool getRechargeThreshOffsetV(uint32_t& mv);
	bool setRechargeThreshOffsetV(uint32_t mv);

	bool getOTGV(uint32_t& mv);
	bool setOTGV(uint32_t mv);

	bool getPrechargeTimer(bq25798_prechg_timer_t& timer);
	bool setPrechargeTimer(bq25798_prechg_timer_t timer);

	bool getOTGLimitA(uint32_t& ma);
	bool setOTGLimitA(uint32_t ma);

	bool getTopOffTimer(bq25798_topoff_timer_t& timer);
	bool setTopOffTimer(bq25798_topoff_timer_t timer);

	bool getTrickleChargeTimerEnable(bool& enable);
	bool setTrickleChargeTimerEnable(bool enable);

	bool getPrechargeTimerEnable(bool& enable);
	bool setPrechargeTimerEnable(bool enable);

	bool getFastChargeTimerEnable(bool& enable);
	bool setFastChargeTimerEnable(bool enable);

	bool getFastChargeTimer(bq25798_chg_timer_t& timer);
	bool setFastChargeTimer(bq25798_chg_timer_t timer);

	bool getTimerHalfRateEnable(bool& enable);
	bool setTimerHalfRateEnable(bool enable);

	bool getAutoOVPBattDischarge(bool& enable);
	bool setAutoOVPBattDischarge(bool enable);

	bool getForceBattDischarge(bool& enable);
	bool setForceBattDischarge(bool enable);

	bool getChargeEnable(bool& enable);
	bool setChargeEnable(bool enable);

	bool getICOEnable(bool& enable);
	bool setICOEnable(bool enable);

	bool getForceICO(bool& enable);
	bool setForceICO(bool enable);

	bool getHIZMode(bool& enable);
	bool setHIZMode(bool enable);

	bool getTerminationEnable(bool& enable);
	bool setTerminationEnable(bool enable);

	bool getBackupModeEnable(bool& enable);
	bool setBackupModeEnable(bool enable);

	bool getBackupModeThresh(bq25798_vbus_backup_t& threshold);
	bool setBackupModeThresh(bq25798_vbus_backup_t threshold);

	bool getVACOVP(bq25798_vac_ovp_t& threshold);
	bool setVACOVP(bq25798_vac_ovp_t threshold);

	bool resetWDT();

	bool getWDT(bq25798_wdt_t& timer);
	bool setWDT(bq25798_wdt_t timer);

	bool getForceDPinsDetection(bool& enable);
	bool setForceDPinsDetection(bool enable);

	bool getAutoDPinsDetection(bool& enable);
	bool setAutoDPinsDetection(bool enable);

	bool getHVDCP12VEnable(bool& enable);
	bool setHVDCP12VEnable(bool enable);

	bool getHVDCP9VEnable(bool& enable);
	bool setHVDCP9VEnable(bool enable);

	bool getHVDCPEnable(bool& enable);
	bool setHVDCPEnable(bool enable);

	bool getShipFETmode(bq25798_sdrv_ctrl_t& mode);
	bool setShipFETmode(bq25798_sdrv_ctrl_t mode);

	bool getShipFET10sDelay(bool& enable);
	bool setShipFET10sDelay(bool enable);

	bool getACenable(bool& enable);
	bool setACenable(bool enable);

	bool getOTGenable(bool& enable);
	bool setOTGenable(bool enable);

	bool getOTGPFM(bool& enable);
	bool setOTGPFM(bool enable);

	bool getForwardPFM(bool& enable);
	bool setForwardPFM(bool enable);

	bool getShipWakeupDelay(bq25798_wkup_dly_t& delay);
	bool setShipWakeupDelay(bq25798_wkup_dly_t delay);

	bool getBATFETLDOprecharge(bool& enable);
	bool setBATFETLDOprecharge(bool enable);

	bool getOTGOOA(bool& enable);
	bool setOTGOOA(bool enable);

	bool getForwardOOA(bool& enable);
	bool setForwardOOA(bool enable);

	bool getACDRV2enable(bool& enable);
	bool setACDRV2enable(bool enable);

	bool getACDRV1enable(bool& enable);
	bool setACDRV1enable(bool enable);

	bool getPWMFrequency(bq25798_pwm_freq_t& frequency);
	bool setPWMFrequency(bq25798_pwm_freq_t frequency);

	bool getStatPinEnable(bool& enable);
	bool setStatPinEnable(bool enable);

	bool getVSYSshortProtect(bool& enable);
	bool setVSYSshortProtect(bool enable);

	bool getVOTG_UVPProtect(bool& enable);
	bool setVOTG_UVPProtect(bool enable);

	bool getIBUS_OCPenable(bool& enable);
	bool setIBUS_OCPenable(bool enable);

	bool getVINDPMdetection(bool& enable);
	bool setVINDPMdetection(bool enable);

	bool getShipFETpresent(bool& enable);
	bool setShipFETpresent(bool enable);

	bool getBatDischargeSenseEnable(bool& enable);
	bool setBatDischargeSenseEnable(bool enable);

	bool getBatDischargeA(bq25798_ibat_reg_t& current);
	bool setBatDischargeA(bq25798_ibat_reg_t current);

	bool getIINDPMenable(bool& enable);
	bool setIINDPMenable(bool enable);

	bool getExtILIMpin(bool& enable);
	bool setExtILIMpin(bool enable);

	bool getBatDischargeOCPenable(bool& enable);
	bool setBatDischargeOCPenable(bool enable);

	bool getVINDPM_VOCpercent(bq25798_voc_pct_t& percentage);
	bool setVINDPM_VOCpercent(bq25798_voc_pct_t percentage);

	bool getVOCdelay(bq25798_voc_dly_t& delay);
	bool setVOCdelay(bq25798_voc_dly_t delay);

	bool getVOCrate(bq25798_voc_rate_t& rate);
	bool setVOCrate(bq25798_voc_rate_t rate);

	bool getMPPTenable(bool& enable);
	bool setMPPTenable(bool enable);

	bool getThermRegulationThresh(bq25798_treg_t& threshold);
	bool setThermRegulationThresh(bq25798_treg_t threshold);

	bool getThermShutdownThresh(bq25798_tshut_t& threshold);
	bool setThermShutdownThresh(bq25798_tshut_t threshold);

	bool getVBUSpulldown(bool& enable);
	bool setVBUSpulldown(bool enable);

	bool getVAC1pulldown(bool& enable);
	bool setVAC1pulldown(bool enable);

	bool getVAC2pulldown(bool& enable);
	bool setVAC2pulldown(bool enable);

	bool getBackupACFET1on(bool& enable);
	bool setBackupACFET1on(bool enable);

	bool reset();

};
