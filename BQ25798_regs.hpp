/*
 * BQ25798_regs.hpp
 *
 *  Created on: 11 Aug 2026
 *      Author: pavloha
 */

#pragma once


#define BQ25798_DEFAULT_ADDR 0x6B ///< Default I2C address

// Register definitions
#define BQ25798_REG_MINIMAL_SYSTEM_VOLTAGE 0x00 ///< Minimal System Voltage
#define BQ25798_REG_CHARGE_VOLTAGE_LIMIT 0x01   ///< Charge Voltage Limit
#define BQ25798_REG_CHARGE_CURRENT_LIMIT 0x03   ///< Charge Current Limit
#define BQ25798_REG_INPUT_VOLTAGE_LIMIT 0x05    ///< Input Voltage Limit
#define BQ25798_REG_INPUT_CURRENT_LIMIT 0x06    ///< Input Current Limit
#define BQ25798_REG_PRECHARGE_CONTROL 0x08      ///< Precharge Control
#define BQ25798_REG_TERMINATION_CONTROL 0x09    ///< Termination Control
#define BQ25798_REG_RECHARGE_CONTROL 0x0A       ///< Re-charge Control
#define BQ25798_REG_VOTG_REGULATION 0x0B        ///< VOTG regulation
#define BQ25798_REG_IOTG_REGULATION 0x0D        ///< IOTG regulation
#define BQ25798_REG_TIMER_CONTROL 0x0E          ///< Timer Control
#define BQ25798_REG_CHARGER_CONTROL_0 0x0F      ///< Charger Control 0
#define BQ25798_REG_CHARGER_CONTROL_1 0x10      ///< Charger Control 1
#define BQ25798_REG_CHARGER_CONTROL_2 0x11      ///< Charger Control 2
#define BQ25798_REG_CHARGER_CONTROL_3 0x12      ///< Charger Control 3
#define BQ25798_REG_CHARGER_CONTROL_4 0x13      ///< Charger Control 4
#define BQ25798_REG_CHARGER_CONTROL_5 0x14      ///< Charger Control 5
#define BQ25798_REG_MPPT_CONTROL 0x15           ///< MPPT Control
#define BQ25798_REG_TEMPERATURE_CONTROL 0x16    ///< Temperature Control
#define BQ25798_REG_NTC_CONTROL_0 0x17          ///< NTC Control 0
#define BQ25798_REG_NTC_CONTROL_1 0x18          ///< NTC Control 1
#define BQ25798_REG_ICO_CURRENT_LIMIT 0x19      ///< ICO Current Limit
#define BQ25798_REG_CHARGER_STATUS_0 0x1B       ///< Charger Status 0
#define BQ25798_REG_CHARGER_STATUS_1 0x1C       ///< Charger Status 1
#define BQ25798_REG_CHARGER_STATUS_2 0x1D       ///< Charger Status 2
#define BQ25798_REG_CHARGER_STATUS_3 0x1E       ///< Charger Status 3
#define BQ25798_REG_CHARGER_STATUS_4 0x1F       ///< Charger Status 4
#define BQ25798_REG_FAULT_STATUS_0 0x20         ///< FAULT Status 0
#define BQ25798_REG_FAULT_STATUS_1 0x21         ///< FAULT Status 1
#define BQ25798_REG_CHARGER_FLAG_0 0x22         ///< Charger Flag 0
#define BQ25798_REG_CHARGER_FLAG_1 0x23         ///< Charger Flag 1
#define BQ25798_REG_CHARGER_FLAG_2 0x24         ///< Charger Flag 2
#define BQ25798_REG_CHARGER_FLAG_3 0x25         ///< Charger Flag 3
#define BQ25798_REG_FAULT_FLAG_0 0x26           ///< FAULT Flag 0
#define BQ25798_REG_FAULT_FLAG_1 0x27           ///< FAULT Flag 1
#define BQ25798_REG_CHARGER_MASK_0 0x28         ///< Charger Mask 0
#define BQ25798_REG_CHARGER_MASK_1 0x29         ///< Charger Mask 1
#define BQ25798_REG_CHARGER_MASK_2 0x2A         ///< Charger Mask 2
#define BQ25798_REG_CHARGER_MASK_3 0x2B         ///< Charger Mask 3
#define BQ25798_REG_FAULT_MASK_0 0x2C           ///< FAULT Mask 0
#define BQ25798_REG_FAULT_MASK_1 0x2D           ///< FAULT Mask 1
#define BQ25798_REG_ADC_CONTROL 0x2E            ///< ADC Control
#define BQ25798_REG_ADC_FUNCTION_DISABLE_0 0x2F ///< ADC Function Disable 0
#define BQ25798_REG_ADC_FUNCTION_DISABLE_1 0x30 ///< ADC Function Disable 1
#define BQ25798_REG_IBUS_ADC 0x31               ///< IBUS ADC
#define BQ25798_REG_IBAT_ADC 0x33               ///< IBAT ADC
#define BQ25798_REG_VBUS_ADC 0x35               ///< VBUS ADC
#define BQ25798_REG_VAC1_ADC 0x37               ///< VAC1 ADC
#define BQ25798_REG_VAC2_ADC 0x39               ///< VAC2 ADC
#define BQ25798_REG_VBAT_ADC 0x3B               ///< VBAT ADC
#define BQ25798_REG_VSYS_ADC 0x3D               ///< VSYS ADC
#define BQ25798_REG_TS_ADC 0x3F                 ///< TS ADC
#define BQ25798_REG_TDIE_ADC 0x41               ///< TDIE ADC
#define BQ25798_REG_DPLUS_ADC 0x43              ///< D+ ADC
#define BQ25798_REG_DMINUS_ADC 0x45             ///< D- ADC
#define BQ25798_REG_DPDM_DRIVER 0x47            ///< DPDM Driver
#define BQ25798_REG_PART_INFORMATION 0x48       ///< Part Information

/*!
 * @brief Battery voltage threshold for precharge to fast charge transition
 */
typedef enum {
  BQ25798_VBAT_LOWV_15_PERCENT = 0x00,   ///< 15% of VREG
  BQ25798_VBAT_LOWV_62_2_PERCENT = 0x01, ///< 62.2% of VREG
  BQ25798_VBAT_LOWV_66_7_PERCENT = 0x02, ///< 66.7% of VREG
  BQ25798_VBAT_LOWV_71_4_PERCENT = 0x03  ///< 71.4% of VREG (default)
} bq25798_vbat_lowv_t;

/*!
 * @brief Battery cell count selection
 */
typedef enum {
  BQ25798_CELL_COUNT_1S = 0x00, ///< 1 cell
  BQ25798_CELL_COUNT_2S = 0x01, ///< 2 cells
  BQ25798_CELL_COUNT_3S = 0x02, ///< 3 cells
  BQ25798_CELL_COUNT_4S = 0x03  ///< 4 cells
} bq25798_cell_count_t;

/*!
 * @brief Battery recharge deglitch time
 */
typedef enum {
  BQ25798_TRECHG_64MS = 0x00,   ///< 64ms
  BQ25798_TRECHG_256MS = 0x01,  ///< 256ms
  BQ25798_TRECHG_1024MS = 0x02, ///< 1024ms (default)
  BQ25798_TRECHG_2048MS = 0x03  ///< 2048ms
} bq25798_trechg_time_t;

/*!
 * @brief Precharge safety timer setting
 */
typedef enum {
  BQ25798_PRECHG_TMR_2HR = 0x00,  ///< 2 hours (default)
  BQ25798_PRECHG_TMR_0_5HR = 0x01 ///< 0.5 hours
} bq25798_prechg_timer_t;

/*!
 * @brief Top-off timer control
 */
typedef enum {
  BQ25798_TOPOFF_TMR_DISABLED = 0x00, ///< Disabled (default)
  BQ25798_TOPOFF_TMR_15MIN = 0x01,    ///< 15 minutes
  BQ25798_TOPOFF_TMR_30MIN = 0x02,    ///< 30 minutes
  BQ25798_TOPOFF_TMR_45MIN = 0x03     ///< 45 minutes
} bq25798_topoff_timer_t;

/*!
 * @brief Fast charge timer setting
 */
typedef enum {
  BQ25798_CHG_TMR_5HR = 0x00,  ///< 5 hours
  BQ25798_CHG_TMR_8HR = 0x01,  ///< 8 hours
  BQ25798_CHG_TMR_12HR = 0x02, ///< 12 hours (default)
  BQ25798_CHG_TMR_24HR = 0x03  ///< 24 hours
} bq25798_chg_timer_t;

/*!
 * @brief Backup mode threshold setting (percentage of VINDPM)
 */
typedef enum {
  BQ25798_VBUS_BACKUP_40_PERCENT = 0x00, ///< 40% of VINDPM
  BQ25798_VBUS_BACKUP_60_PERCENT = 0x01, ///< 60% of VINDPM
  BQ25798_VBUS_BACKUP_80_PERCENT = 0x02, ///< 80% of VINDPM (default)
  BQ25798_VBUS_BACKUP_100_PERCENT = 0x03 ///< 100% of VINDPM
} bq25798_vbus_backup_t;

/*!
 * @brief VAC overvoltage protection setting
 */
typedef enum {
  BQ25798_VAC_OVP_26V = 0x00, ///< 26V
  BQ25798_VAC_OVP_22V = 0x01, ///< 22V
  BQ25798_VAC_OVP_12V = 0x02, ///< 12V
  BQ25798_VAC_OVP_7V = 0x03   ///< 7V (default)
} bq25798_vac_ovp_t;

/*!
 * @brief Watchdog timer setting
 */
typedef enum {
  BQ25798_WDT_DISABLE = 0x00, ///< Disable watchdog
  BQ25798_WDT_0_5S = 0x01,    ///< 0.5 seconds
  BQ25798_WDT_1S = 0x02,      ///< 1 second
  BQ25798_WDT_2S = 0x03,      ///< 2 seconds
  BQ25798_WDT_20S = 0x04,     ///< 20 seconds
  BQ25798_WDT_40S = 0x05,     ///< 40 seconds (default)
  BQ25798_WDT_80S = 0x06,     ///< 80 seconds
  BQ25798_WDT_160S = 0x07     ///< 160 seconds
} bq25798_wdt_t;

/*!
 * @brief Ship FET mode control setting
 */
typedef enum {
  BQ25798_SDRV_IDLE = 0x00,        ///< IDLE (default)
  BQ25798_SDRV_SHUTDOWN = 0x01,    ///< Shutdown Mode
  BQ25798_SDRV_SHIP = 0x02,        ///< Ship Mode
  BQ25798_SDRV_SYSTEM_RESET = 0x03 ///< System Power Reset
} bq25798_sdrv_ctrl_t;

/*!
 * @brief Ship mode wakeup delay setting
 */
typedef enum {
  BQ25798_WKUP_DLY_1S = 0x00,  ///< 1 second (default)
  BQ25798_WKUP_DLY_15MS = 0x01 ///< 15ms
} bq25798_wkup_dly_t;

/*!
 * @brief PWM switching frequency setting
 */
typedef enum {
  BQ25798_PWM_FREQ_1_5MHZ = 0x00, ///< 1.5 MHz
  BQ25798_PWM_FREQ_750KHZ = 0x01  ///< 750 kHz
} bq25798_pwm_freq_t;

/*!
 * @brief Battery discharge current regulation setting
 */
typedef enum {
  BQ25798_IBAT_REG_3A = 0x00,     ///< 3A
  BQ25798_IBAT_REG_4A = 0x01,     ///< 4A
  BQ25798_IBAT_REG_5A = 0x02,     ///< 5A
  BQ25798_IBAT_REG_DISABLE = 0x03 ///< Disable (default)
} bq25798_ibat_reg_t;

/*!
 * @brief VINDPM VOC percentage setting
 */
typedef enum {
  BQ25798_VOC_PCT_56_25 = 0x00, ///< 56.25% (0.5625)
  BQ25798_VOC_PCT_62_5 = 0x01,  ///< 62.5% (0.625)
  BQ25798_VOC_PCT_68_75 = 0x02, ///< 68.75% (0.6875)
  BQ25798_VOC_PCT_75 = 0x03,    ///< 75% (0.75)
  BQ25798_VOC_PCT_81_25 = 0x04, ///< 81.25% (0.8125)
  BQ25798_VOC_PCT_87_5 = 0x05,  ///< 87.5% (0.875) (default)
  BQ25798_VOC_PCT_93_75 = 0x06, ///< 93.75% (0.9375)
  BQ25798_VOC_PCT_100 = 0x07    ///< 100% (1.0)
} bq25798_voc_pct_t;

/*!
 * @brief VOC delay time setting
 */
typedef enum {
  BQ25798_VOC_DLY_50MS = 0x00,  ///< 50ms
  BQ25798_VOC_DLY_300MS = 0x01, ///< 300ms (default)
  BQ25798_VOC_DLY_2S = 0x02,    ///< 2 seconds
  BQ25798_VOC_DLY_5S = 0x03     ///< 5 seconds
} bq25798_voc_dly_t;

/*!
 * @brief VOC measurement rate setting
 */
typedef enum {
  BQ25798_VOC_RATE_30S = 0x00,   ///< 30 seconds
  BQ25798_VOC_RATE_2MIN = 0x01,  ///< 2 minutes (default)
  BQ25798_VOC_RATE_10MIN = 0x02, ///< 10 minutes
  BQ25798_VOC_RATE_30MIN = 0x03  ///< 30 minutes
} bq25798_voc_rate_t;

/*!
 * @brief Thermal regulation threshold setting
 */
typedef enum {
  BQ25798_TREG_60C = 0x00,  ///< 60°C
  BQ25798_TREG_80C = 0x01,  ///< 80°C
  BQ25798_TREG_100C = 0x02, ///< 100°C
  BQ25798_TREG_120C = 0x03  ///< 120°C (default)
} bq25798_treg_t;

/*!
 * @brief Thermal shutdown threshold setting
 */
typedef enum {
  BQ25798_TSHUT_150C = 0x00, ///< 150°C (default)
  BQ25798_TSHUT_130C = 0x01, ///< 130°C
  BQ25798_TSHUT_120C = 0x02, ///< 120°C
  BQ25798_TSHUT_85C = 0x03   ///< 85°C
} bq25798_tshut_t;

