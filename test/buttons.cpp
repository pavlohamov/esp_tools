/*
 * test.cpp
 *
 *  Created on: 1 Aug 2026
 *      Author: pavloha
 */


#include "Buttons.hpp"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

#include <list>

#include "esp_log.h"
static const char *TAG = "TEST::Buttons";

struct test_evt {
	int btn;
	int evt;
};

static void onBtnEvent(int btn, int evt, void *arg) {
	QueueHandle_t q = (QueueHandle_t)arg;
	test_evt e = {
		.btn = btn,
		.evt = evt,
	};
	xQueueSend(q, &e, 0);
}

static bool wait4(QueueHandle_t q, int code, int state, int event, int wait_ms) {
	test_evt e;

	const int bs = Buttons::Wrapper::instance().state(0);

	if (state >= 0 && bs != state) {
		ESP_LOGE(TAG, "wrong state %d want %d ev %d", bs, state, event);
		return false;
	}

	if (!xQueueReceive(q, &e, pdMS_TO_TICKS(wait_ms))) {
		ESP_LOGE(TAG, "no event");
		return false;
	}

	if (e.btn != code) {
		ESP_LOGE(TAG, "wrong code %x want %x", e.btn, code);
		return false;
	}

	if (e.evt != event) {
		ESP_LOGE(TAG, "wrong event %d want %d", e.evt, event);
		return false;
	}

	if (xQueueReceive(q, &e, 0)) {
		ESP_LOGE(TAG, "q not empty");
		return false;
	}

	return true;
}

static bool base() {

	test_evt e;
	QueueHandle_t q = xQueueCreate(16, sizeof(e));
	if (!q) {
		ESP_LOGE(TAG, "no mem for q");
		return false;
	}

	Buttons::Wrapper& wrap = Buttons::Wrapper::instance();

	static const gpio_num_t testGp = GPIO_NUM_0;
	static const int code = 0x1234;
	auto b = wrap.add(testGp, true, code, onBtnEvent, q);

	ESP_ERROR_CHECK(gpio_od_enable(testGp));
	ESP_ERROR_CHECK(gpio_output_enable(testGp));

	const int debounce = wrap.debounce_ms(0) * 12 / 10;
	const int first_repeat = wrap.first_repeat_ms(0) * 12 / 10;
	const int next_repeat = wrap.next_repeat_ms(0) * 12 / 10;

	{
		ESP_ERROR_CHECK(gpio_set_level(testGp, 0));
		if (!wait4(q, code, 0, Buttons::PRESS, debounce))
			return false;

		ESP_ERROR_CHECK(gpio_set_level(testGp, 1));
		if (!wait4(q, code, -1, Buttons::RELEASE, debounce))
			return false;

		ESP_LOGI(TAG, "%s: PASS", "Press->Release");
	}

	{
		ESP_ERROR_CHECK(gpio_set_level(testGp, 0));
		if (!wait4(q, code, 0, Buttons::PRESS, debounce))
			return false;

		if (!wait4(q, code, 1, Buttons::FIRST_REPEAT, first_repeat))
			return false;

		ESP_ERROR_CHECK(gpio_set_level(testGp, 1));
		if (!wait4(q, code, -1, Buttons::RELEASE, debounce))
			return false;

		ESP_LOGI(TAG, "%s: PASS", "Press->Repeat->Release");
	}

	{
		ESP_ERROR_CHECK(gpio_set_level(testGp, 0));
		if (!wait4(q, code, 0, Buttons::PRESS, debounce))
			return false;

		if (!wait4(q, code, 1, Buttons::FIRST_REPEAT, first_repeat))
			return false;

		if (!wait4(q, code, 1, Buttons::REPEAT, next_repeat))
			return false;

		ESP_ERROR_CHECK(gpio_set_level(testGp, 1));
		if (!wait4(q, code, -1, Buttons::RELEASE, debounce))
			return false;

		ESP_LOGI(TAG, "%s: PASS", "Press->Repeat->Repeat->Release");
	}

	{
		ESP_ERROR_CHECK(gpio_set_level(testGp, 0));
		vTaskDelay(pdMS_TO_TICKS(debounce / 2));
		if (xQueueReceive(q, &e, 0)) {
			ESP_LOGE(TAG, "got event %x %d during debounce", e.btn, e.evt);
			return false;
		}
		ESP_ERROR_CHECK(gpio_set_level(testGp, 1));
		vTaskDelay(pdMS_TO_TICKS(debounce * 2));
		if (xQueueReceive(q, &e, 0)) {
			ESP_LOGE(TAG, "got event %x %d after debounce", e.btn, e.evt);
			return false;
		}

		ESP_LOGI(TAG, "%s: PASS", "Press->Debounce");
	}

	wrap.del(b);

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);

	vQueueDelete(q);
	return true;
}


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif


bool Test_Buttons() {
	static const auto tests = {
		base,
	};

	for (auto test: tests) {
		if (!test())
			return false;
	}

	return true;
}

