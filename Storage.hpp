/*
 * Storage.hpp
 *
 *  Created on: Apr 22, 2025
 *      Author: pavlo
 */

#pragma once

#include "Singleton.hpp"

#include <vector>
#include <string>

#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_handle.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "MutexFreeRtos.hpp"

class Storage: public NonCopyable {
public:
	Storage(const char *label);

	[[nodiscard]] int init();

	[[nodiscard]] int get(const char *key, std::string &val) noexcept;
	[[nodiscard]] int set(const char *key, const std::string &val) noexcept;

	template<typename T>
	[[nodiscard]] int get(const char *key, T &value) noexcept {
		return _handle->get_item(key, value);
	}
	template<typename T>
	[[nodiscard]] int set(const char *key, const T &value) noexcept {
		return _handle->set_item(key, value);
	}

	[[nodiscard]] int commit() noexcept;

private:

	[[nodiscard]] bool checktype(const char *key, nvs_type_t type) noexcept;
	[[nodiscard]] int size(const char *key) noexcept;

private:
	const char *const _lbl;
	std::unique_ptr<nvs::NVSHandle> _handle;
	MutexRecursiveFr _lock;
};


class StorageWrapper: public Singleton<StorageWrapper> {
public:
	constexpr Storage& nvs() noexcept { return _nvs; }

private:
	Storage _nvs;

	friend class Singleton<StorageWrapper>;
	StorageWrapper() : _nvs("nvs") {}
};
