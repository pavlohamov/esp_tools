/*
 * main.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */



#include "esp_log.h"
static const char *TAG = "TEST";

#include "Singleton.hpp"
#include <initializer_list>

bool Test_Runnable();
bool Test_Storage();
bool Test_Buttons();

extern "C" void app_main(void) {
	static const auto tests = {
		Test_Runnable,
//		Test_Storage,
		Test_Buttons,
	};
	ESP_LOGI(TAG, "START");

	for (auto test: tests) {
		if (!test())
			return;
	}

	ESP_LOGI(TAG, "ALL PASS");
}

