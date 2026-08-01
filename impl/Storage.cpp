/*
 * Storage.cpp
 *
 *  Created on: Apr 22, 2025
 *      Author: pavlo
 */
#include <string.h>
#include <mutex>

#include "Storage.hpp"
#include "nvs_flash.h"

#include "esp_log.h"
static const char *TAG = "storage";

#define NVS_MAX_LENGTH (4096) // actually 4000

Storage::Storage(const char *label) : _lbl(label), _handle(nullptr) { }

int Storage::init() {

	if (_handle)
		return 0;

	std::lock_guard<MutexRecursiveFr> guard(_lock);

	int err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

    _handle = nvs::open_nvs_handle(_lbl, NVS_READWRITE, &err);
	ESP_ERROR_CHECK(err);

	return err;
}

int Storage::get(const char *key, std::string &val) {

	std::lock_guard<MutexRecursiveFr> guard(_lock);
	size_t size = 0;
	int rv = _handle->get_item_size(nvs::ItemType::SZ, key, size);
	if (rv)
		return false;

	val.reserve(size + 1);
	return _handle->get_string(key, val.data(), size);
}

int Storage::set(const char *key, const std::string &val) {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
	return _handle->set_string(key, val.c_str());
}

int Storage::commit() {
	std::lock_guard<MutexRecursiveFr> guard(_lock);
	return _handle->commit();
}
