/*
 * test.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */


#include "Wifi.hpp"
#include "esp_heap_caps.h"

#include "esp_log.h"
static const char *TAG = "TEST::wifi";


static bool init() {
	Wifi& wifi = Wifi::instance();
	if (!wifi.init()) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}
	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}


static bool scan() {
	Wifi& wifi = Wifi::instance();
	if (!wifi.init()) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}

	SemaphoreHandle_t done = xSemaphoreCreateBinary();
	auto handle = wifi.scan_subscribe([&](const wifi_ap_record_t *recs, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			const wifi_ap_record_t *ap = recs + i;
			ESP_LOGI(TAG, "%d/%d '%.32s' %d %ddbm %.2s %d%d", i + 1, count,
				ap->ssid, ap->authmode, ap->rssi, ap->country.cc, ap->ftm_initiator, ap->ftm_responder);
		}
		xSemaphoreGive(done);
	});
	if (!wifi.scan()) {
		ESP_LOGE(TAG, "%s: scan FAIL", __FUNCTION__);
		vSemaphoreDelete(done);
		(void)wifi.scan_unsubscribe(handle);
		return false;
	}

	const bool timedout = !xSemaphoreTake(done, pdMS_TO_TICKS(5000));
	vSemaphoreDelete(done);
	if (!wifi.scan_unsubscribe(handle)) {
		ESP_LOGE(TAG, "%s: unsibscribe FAIL", __FUNCTION__);
		return false;
	}

	if (timedout) {
		ESP_LOGE(TAG, "%s: timeout FAIL", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}

static bool start_ap() {

	Wifi& wifi = Wifi::instance();
	if (!wifi.init()) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}

	const wifi_config_t cfg = {
		.ap = {
			.ssid = "test",
			.password = "",
			.ssid_len = 4,
			.channel = 0,
			.authmode = WIFI_AUTH_OPEN,
			.ssid_hidden = 0,
			.max_connection = CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM,
			.beacon_interval = 0,
			.csa_count = 0,
			.dtim_period = 0,
			.pairwise_cipher = WIFI_CIPHER_TYPE_NONE,
			.ftm_responder = true,
			.pmf_cfg = {
				.capable = true,
				.required = false,
			},
			.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED,
			.transition_disable = 0,
			.sae_ext = 1,
			.wpa3_compatible_mode = 0,
			.reserved = 0,
			.bss_max_idle_cfg = {
				.period = 0,
				.protected_keep_alive = 0,
			},
			.gtk_rekey_interval = 0,
		},
	};

    if (!wifi.ap_start(cfg)) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}

	SemaphoreHandle_t done = xSemaphoreCreateBinary();
    auto handle = wifi.ap_subscribe([&](bool isWifi, int32_t event_id, void *data) {
		if (isWifi && event_id == WIFI_EVENT_AP_START) {
			xSemaphoreGive(done);
			return;
		}
		ESP_LOGI(TAG, "something %d %d", isWifi, event_id);
	});

	const bool timedout = !xSemaphoreTake(done, pdMS_TO_TICKS(2000));
	vSemaphoreDelete(done);
	if (!wifi.ap_unsubscribe(handle)) {
		ESP_LOGE(TAG, "%s: unsibscribe FAIL", __FUNCTION__);
		return false;
	}

	if (timedout) {
		ESP_LOGE(TAG, "%s: timeout FAIL", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}

static bool stop_ap() {
	Wifi& wifi = Wifi::instance();
	if (!wifi.init()) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}

	if (!wifi.ap_stop()) {
		ESP_LOGE(TAG, "%s:ap stop FAIL", __FUNCTION__);
		return false;
	}
	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}


static bool deinit() {
	Wifi& wifi = Wifi::instance();
	if (!wifi.deinit()) {
		ESP_LOGE(TAG, "%s: FAIL", __FUNCTION__);
		return false;
	}
	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif


bool Test_Wifi() {
	static const auto tests = {
		init,
		scan,
		start_ap,
		stop_ap,
		// connect,
		deinit,
	};

	static const uint32_t caps[] = {
		MALLOC_CAP_DEFAULT,
		MALLOC_CAP_8BIT,
		MALLOC_CAP_DMA,
		MALLOC_CAP_SPIRAM,
		MALLOC_CAP_INTERNAL,
	};

	size_t memfree[ARRAY_SIZE(caps)];
	for (size_t i = 0; i < ARRAY_SIZE(caps); ++i)
		memfree[i] = heap_caps_get_free_size(caps[i]);

	for (auto test: tests) {
		if (!test())
			return false;
	}

	for (size_t i = 0; i < ARRAY_SIZE(caps); ++i) {
		const size_t nowfree = heap_caps_get_free_size(caps[i]);
		if (nowfree == memfree[i])
			continue;
		ESP_LOGE(TAG, "mem %p was %zu now %zu %d", caps[i], memfree[i], nowfree, memfree[i] - nowfree);
		// return false;
	}
	return true;
}
