#pragma once

#include <list>
#include <functional>

#include "Singleton.hpp"
#include "MutexFreeRtos.hpp"

#include "esp_wifi.h"
#include "esp_event_base.h"

class Wifi: public Singleton<Wifi> {
public:
    using OnApAction = std::function<void(bool isWifi, int32_t event_id, void *data)>;
    using ApHandle   = std::list<OnApAction>::iterator;

    using OnScanResult = std::function<void(const wifi_ap_record_t *recs, size_t count)>;
    using ScanHandle   = std::list<OnScanResult>::iterator;

    [[nodiscard]] bool init() noexcept;
    [[nodiscard]] bool deinit() noexcept;

    [[nodiscard]] bool connect(const wifi_config_t& cfg) noexcept;
    [[nodiscard]] bool disconnect() noexcept;

    [[nodiscard]] bool ap_start(const wifi_config_t& cfg) noexcept;
    [[nodiscard]] bool ap_stop() noexcept;

    [[nodiscard]] ApHandle ap_subscribe(OnApAction cb) noexcept;
    [[nodiscard]] bool ap_unsubscribe(ApHandle cb) noexcept;

    [[nodiscard]] ScanHandle scan_subscribe(OnScanResult cb) noexcept;
    [[nodiscard]] bool scan_unsubscribe(ScanHandle cb) noexcept;

    [[nodiscard]] bool scan() noexcept;
private:
	friend class Singleton<Wifi>;

    esp_netif_t* _netif_sta = nullptr;
    esp_netif_t* _netif_ap = nullptr;
    bool _inited = false;
    bool _ipready = false;
	MutexRecursiveFr _lock;
	MutexRecursiveFr _cblock;
    std::list<OnApAction> _ap_listeners;
    std::list<OnScanResult> _scanners;

private:
    static void onEventWifi(void* user_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) ;
    static void onEventIp(void* user_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
};

