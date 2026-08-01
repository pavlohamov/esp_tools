/*
 * test.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */


#include "Storage.hpp"

#include "esp_heap_caps.h"

#include <list>

#include "esp_log.h"
static const char *TAG = "TEST::Storage";


template<typename T>
static bool check(Storage &st, const char *name, const T value) {
	if (st.set(name, value)) {
		ESP_LOGE(TAG, "not Set %s", name);
		return false;
	}

	T backread = ~value;
	if (st.get(name, backread)) {
		ESP_LOGE(TAG, "not Get %s", name);
		return false;
	}
	if (backread != value) {
		ESP_LOGE(TAG, "not %s", name);
		return false;
	}
	return true;
}

static bool base(Storage &st) {
	if (st.init()) {
		ESP_LOGE(TAG, "not Init");
		return false;
	}

	if (!check(st, "u8", 0x88))
		return false;

	if (!check(st, "i8", 0x44))
		return false;

	if (!check(st, "u16", 0x1616))
		return false;

	if (!check(st, "i16", 0x6161))
		return false;

	if (!check(st, "u32", 0x32323232))
		return false;

	if (!check(st, "i32", 0x23232323))
		return false;

	if (!check(st, "u64", 0x6464646464646464))
		return false;

	if (!check(st, "i64", 0x4646464646464646))
		return false;

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif


bool Test_Storage() {
	static const auto tests = {
		base,
	};
	Storage &st = StorageWrapper::instance().nvs();

	for (auto test: tests) {
		if (!test(st))
			return false;
	}

	return true;
}
