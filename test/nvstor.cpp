/*
 * test.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#include "NvStorage.hpp"

#include "esp_log.h"
static const char *TAG = "TEST::nvstorage";

bool Test_NvStorage() {

	typedef struct {
		uint32_t cycles;
		uint32_t uptime;
	} OdoSave_t;

	struct FlashPart {
		uint8_t buffer[64] = {};
		bool read(off_t off, void* data, size_t size) {
			// ESP_LOGI(TAG, "read @%X", off);
			memcpy(data, buffer + off, size);
			return true;
		}
		bool write(off_t off, const void* data, size_t size) {
			// ESP_LOGI(TAG, "write @%X", off);
			memcpy(buffer + off, data, size);
			return true;
		}
		bool erase(size_t size) {
			// ESP_LOGI(TAG, "erase @%X", size);
			memset(buffer, 0, size);
			return true;
		}
	};
	FlashPart fp;
	NvStorage<OdoSave_t, sizeof(fp.buffer), FlashPart> stor{fp};

	OdoSave_t odo;
	if (stor.find(odo)) {
		ESP_LOGE(TAG, "first read must fail");
		return false;
	}
	odo.cycles = 0xAAAAAAAA;
	odo.uptime = 0xBBBBBBBB;
	for (int i = 0; i < 16; ++i) {
		if (!stor.set(odo)) {
			ESP_LOGE(TAG, "set fail");
			return false;
		}
		if (!stor.find(odo)) {
			ESP_LOGE(TAG, "get fail");
			return false;
		}
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);

	return true;
}
