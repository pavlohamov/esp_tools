
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <mutex>

#include "Wifi.hpp"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_crc.h"
#include "esp_random.h"
#include "esp_timer.h"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include "esp_log.h"
static const char *TAG = "mWiFi";


void Wifi::onEventWifi(void* user_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

	Wifi *me = static_cast<Wifi*>(user_arg);
	switch (event_id) {
		case WIFI_EVENT_WIFI_READY: {
			ESP_LOGD(TAG, "WiFi Ready");
		} break;
		case WIFI_EVENT_SCAN_DONE: {
		    uint16_t count = 0;
		    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&count));
			ESP_LOGI(TAG, "Scan Done. Found %d", count);
			if (count) {
				wifi_ap_record_t *aps = static_cast<wifi_ap_record_t*>(malloc(count * sizeof(wifi_ap_record_t)));
				if (!aps)
					break;
				if (esp_wifi_scan_get_ap_records(&count, aps)) {
					ESP_LOGI(TAG, "failed to get scan results");
					esp_wifi_clear_ap_list();
					free(aps);
					break;
				}

				for (uint16_t i = 0; i < count; ++i) {
					wifi_ap_record_t *ap = aps + i;
					ESP_LOGV(TAG, "%d/%d '%.32s' %d %ddbm %.2s %d%d", i + 1, count,
						ap->ssid, ap->authmode, ap->rssi, ap->country.cc, ap->ftm_initiator, ap->ftm_responder);
				}
				{
					std::lock_guard<MutexRecursiveFr> guard(me->_cblock);
					for (auto& cb: me->_scanners)
						cb(aps, count);
				}
				// todo: callbacks
				free(aps);
			}
		} break;

		case WIFI_EVENT_STA_START: {
			ESP_LOGI(TAG, "WiFi STA Ready. Scanning");
		    // esp_wifi_scan_start(NULL, false);
		} break;
		case WIFI_EVENT_STA_STOP: {
			ESP_LOGI(TAG, "Station stop");
		} break;
		case WIFI_EVENT_STA_CONNECTED: {
	        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
			ESP_LOGI(TAG, "Station connected to AP");
		    wifi_ftm_initiator_cfg_t ftmi_cfg = {
				.resp_mac = {},
				.channel = 0,
		        .frm_count = 32,
		        .burst_period = 2,
		    };
	        memcpy(ftmi_cfg.resp_mac, event->bssid, sizeof(ftmi_cfg.resp_mac));
	        ftmi_cfg.channel = event->channel;
	        esp_wifi_ftm_initiate_session(&ftmi_cfg);
		} break;
		case WIFI_EVENT_STA_DISCONNECTED: {
			wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*)event_data;
			ESP_LOGI(TAG, "Station disconnected from AP %d", event->reason);
			esp_wifi_scan_start(NULL, false);
			me->_ipready = 0;
		} break;
		case WIFI_EVENT_STA_AUTHMODE_CHANGE: {
			ESP_LOGI(TAG, "the auth mode of AP connected by device's station changed");
		} break;
		case WIFI_EVENT_AP_START: {
			ESP_LOGI(TAG, "Soft-AP start");
		} break;
		case WIFI_EVENT_AP_STOP: {
			ESP_LOGI(TAG, "Soft-AP stop");
		} break;
		case WIFI_EVENT_AP_STACONNECTED: {
			ESP_LOGI(TAG, "a station connected to Soft-AP");
		} break;
		case WIFI_EVENT_AP_STADISCONNECTED: {
			ESP_LOGI(TAG, "a station disconnected from Soft-AP");
		} break;
		case WIFI_EVENT_AP_PROBEREQRECVED: {
			ESP_LOGI(TAG, "Receive probe request packet in soft-AP interface");
		} break;
		case WIFI_EVENT_FTM_REPORT: {
	        wifi_event_ftm_report_t *event = (wifi_event_ftm_report_t *)event_data;
			ESP_LOGI(TAG, "FTM: %" PRId32 " nSec, est Distance - %" PRId32 ".%02" PRId32 " m", event->rtt_est, event->dist_est / 100, event->dist_est % 100);
		} break;
		default: {
			ESP_LOGD(TAG, "event %ld", event_id);
		} break;
	}

	{
		std::lock_guard<MutexRecursiveFr> guard(me->_cblock);
		for (auto& cb: me->_ap_listeners)
			cb(true, event_id, event_data);
	}

}

void Wifi::onEventIp(void* user_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	Wifi *me = static_cast<Wifi*>(user_arg);
	switch (event_id) {
		case IP_EVENT_STA_GOT_IP: {
			ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
			char buff[32];
			snprintf(buff, sizeof(buff), "STA " IPSTR, IP2STR(&event->ip_info.ip));
			ESP_LOGI(TAG, "Got ip %s", buff);
			me->_ipready = 1;
		} break;
		case IP_EVENT_ASSIGNED_IP_TO_CLIENT: {
			ip_event_assigned_ip_to_client_t *event = (ip_event_assigned_ip_to_client_t*)event_data;
			char buff[32];
			snprintf(buff, sizeof(buff), "AP " IPSTR, IP2STR(&event->ip));
			ESP_LOGI(TAG, "Give ip %s", buff);
		} break;
		case IP_EVENT_GOT_IP6:
		case IP_EVENT_ETH_GOT_IP:
		case IP_EVENT_PPP_GOT_IP:{
			ESP_LOGI(TAG, "Got ip");
		} break;
		case IP_EVENT_STA_LOST_IP:
		case IP_EVENT_ETH_LOST_IP:
		case IP_EVENT_PPP_LOST_IP: {
			ESP_LOGI(TAG, "lost ip");
			me->_ipready = 0;
		} break;
	}

	{
		std::lock_guard<MutexRecursiveFr> guard(me->_cblock);
		for (auto& cb: me->_ap_listeners)
			cb(false, event_id, event_data);
	}
}

bool Wifi::init() noexcept {

	std::lock_guard<MutexRecursiveFr> guard(_lock);

    if (_inited)
        return true;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    _netif_sta = esp_netif_create_default_wifi_sta();
    _netif_ap = esp_netif_create_default_wifi_ap();
    assert(_netif_sta);
    assert(_netif_ap);

    esp_netif_set_hostname(_netif_sta, "my name");
    esp_netif_set_hostname(_netif_ap, "my name");

    esp_netif_attach_wifi_station(_netif_sta);
    esp_netif_attach_wifi_ap(_netif_ap);

//    esp_set_default_wifi_handers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEventWifi, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, onEventIp, this));

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();

	esp_wifi_set_ps(WIFI_PS_NONE); // disable power save - speedup wifi
    _inited = true;

    return true;
}

bool Wifi::deinit() noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
    if (!_inited) {
        return true;
	}
	esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, onEventWifi);
	esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, onEventIp);

	esp_wifi_stop();
	esp_wifi_deinit();

    esp_netif_destroy_default_wifi(_netif_sta);
    esp_netif_destroy_default_wifi(_netif_ap);
	_netif_sta = nullptr;
	_netif_ap = nullptr;
	_inited = false;
	esp_netif_deinit();

	return true;
}

bool Wifi::connect(const wifi_config_t& cfg) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
	esp_wifi_clear_fast_connect();
	wifi_config_t cpy = cfg;
	int rv = esp_wifi_set_config(WIFI_IF_STA, &cpy);
	if (rv) {
		ESP_LOGE(TAG, "Can't set cfg. %X", rv);
		return rv == ESP_OK;
	}

	wifi_mode_t mode;
	rv = esp_wifi_get_mode(&mode);
	if (rv != ESP_OK) {
		ESP_LOGE(TAG, "Can't get mode %X", rv);
		return false;
	}
	if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
		rv = esp_wifi_set_mode(WIFI_MODE_STA);
		if (rv != ESP_OK) {
			ESP_LOGE(TAG, "Can't set mode %X", rv);
			return false;
		}
	}

	rv = esp_wifi_connect();
	if (rv) {
		ESP_LOGE(TAG, "Can't connect cfg. %X", rv);
	}
	return rv == ESP_OK;
}

bool Wifi::disconnect() noexcept {
	return esp_wifi_disconnect() == ESP_OK;
}

bool Wifi::ap_start(const wifi_config_t& cfg) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
	wifi_mode_t mode;
	int rv = esp_wifi_get_mode(&mode);
	if (rv != ESP_OK) {
		ESP_LOGE(TAG, "Can't get mode %X", rv);
		return false;
	}
	if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA) {
		rv = esp_wifi_set_mode(WIFI_MODE_AP);
		if (rv != ESP_OK) {
			ESP_LOGE(TAG, "Can't set mode %X", rv);
			return false;
		}
	}

	wifi_config_t cpy = cfg;
	rv = esp_wifi_set_config(WIFI_IF_AP, &cpy);
	if (rv) {
		ESP_LOGE(TAG, "Can't set cfg. %X", rv);
		return rv == ESP_OK;
	}
	return rv == ESP_OK;

}
bool Wifi::ap_stop() noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
	wifi_mode_t mode;
	int rv = esp_wifi_get_mode(&mode);
	if (rv != ESP_OK) {
		ESP_LOGE(TAG, "Can't get mode %X", rv);
		return false;
	}
	if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA)
		return true;

	rv = esp_wifi_set_mode(WIFI_MODE_STA);
	if (rv != ESP_OK) {
		ESP_LOGE(TAG, "Can't set mode %X", rv);
		return false;
	}
	return true;
}

Wifi::ApHandle Wifi::ap_subscribe(OnApAction cb) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_cblock);
	return _ap_listeners.insert(_ap_listeners.end(), std::move(cb));
}

bool Wifi::ap_unsubscribe(ApHandle h) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_cblock);
	_ap_listeners.erase(h);
	return true;
}

Wifi::ScanHandle Wifi::scan_subscribe(OnScanResult cb) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_cblock);
	return _scanners.insert(_scanners.end(), std::move(cb));
}

bool Wifi::scan_unsubscribe(ScanHandle h) noexcept {
	std::lock_guard<MutexRecursiveFr> guard(_cblock);
	_scanners.erase(h);
	return true;
}

bool Wifi::scan() noexcept {
	return esp_wifi_scan_start(NULL, false) == ESP_OK;
}