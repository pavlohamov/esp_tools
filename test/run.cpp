/*
 * test.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */


#include "Runnable.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include <list>

#include "esp_log.h"
static const char *TAG = "TEST::Runnable";

class TestRun: public Runnable {

public:
	TestRun(int32_t &con): Runnable("test"), constructed_(con) {
		lock_ = xSemaphoreCreateBinary();
		++constructed_;
	}
	virtual ~TestRun() {
		vSemaphoreDelete(lock_);
		--constructed_;
	}
	bool given() const { return xSemaphoreTake(lock_, 0); }

protected:
	virtual void run() noexcept {

		while (state() == State::Running) {
			xSemaphoreGive(lock_);
			vTaskDelay(pdMS_TO_TICKS(1));
		}
	}

private:
	SemaphoreHandle_t lock_;
	int32_t& constructed_;
};


static bool base() {
	int32_t con = false;
	auto tr = std::make_unique<TestRun>(con);
	if (!tr) {
		ESP_LOGE(TAG, "%s: not allocated", __FUNCTION__);
		return false;
	}

	if (!con) {
		ESP_LOGE(TAG, "%s: not Constructed", __FUNCTION__);
		return false;
	}

	if (!tr->start(5, 2048, 0)) {
		ESP_LOGE(TAG, "%s: not Started", __FUNCTION__);
		return false;
	}

	vTaskDelay(pdMS_TO_TICKS(2));

	if (!tr->running()) {
		ESP_LOGE(TAG, "%s: not Running", __FUNCTION__);
		return false;
	}

	if (!tr->given()) {
		ESP_LOGE(TAG, "%s: not Given", __FUNCTION__);
		return false;
	}
	tr->join();

	if (tr->state() != Runnable::State::Idle) {
		ESP_LOGE(TAG, "%s: not Joined %d", (int)tr->state());
		return false;
	}

	tr.reset();

	if (con) {
		ESP_LOGE(TAG, "%s: not Destructed", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}

static bool stop() {

	int32_t con = false;
	auto tr = std::make_unique<TestRun>(con);

	if (!tr->start(5, 2048, 0)) {
		ESP_LOGE(TAG, "%s: not Started", __FUNCTION__);
		return false;
	}

	vTaskDelay(pdMS_TO_TICKS(2));

	if (!tr->running()) {
		ESP_LOGE(TAG, "%s: not Running", __FUNCTION__);
		return false;
	}

	if (!tr->stop()) {
		ESP_LOGE(TAG, "%s: not Stopped", __FUNCTION__);
		return false;
	}

	vTaskDelay(pdMS_TO_TICKS(2));

	tr.reset();

	if (con) {
		ESP_LOGE(TAG, "%s: not Destructed", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}

static bool kill() {

	int32_t con = false;
	auto tr = std::make_unique<TestRun>(con);

	if (!tr->start(5, 2048, 0)) {
		ESP_LOGE(TAG, "%s: not Started", __FUNCTION__);
		return false;
	}

	vTaskDelay(pdMS_TO_TICKS(2));

	if (!tr->running()) {
		ESP_LOGE(TAG, "%s: not Running", __FUNCTION__);
		return false;
	}

	vTaskDelay(pdMS_TO_TICKS(2));

	tr.reset();

	if (con) {
		ESP_LOGE(TAG, "%s: not Destructed", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}


static bool cancel() {

	int32_t con = false;
	{
		auto tr = std::make_unique<TestRun>(con);

		if (!tr->start(5, 2048, 0)) {
			ESP_LOGE(TAG, "%s: not Started", __FUNCTION__);
			return false;
		}

		vTaskDelay(pdMS_TO_TICKS(2));

		if (!tr->running()) {
			ESP_LOGE(TAG, "%s: not Running", __FUNCTION__);
			return false;
		}

		vTaskDelay(pdMS_TO_TICKS(2));

		tr->cancel();

		if (tr->state() != Runnable::State::Idle) {
			ESP_LOGE(TAG, "%s: not idle %d", (int)tr->state());
			return false;
		}
	}

	if (con) {
		ESP_LOGE(TAG, "%s: not Destructed", __FUNCTION__);
		return false;
	}

	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);
	return true;
}


static bool spawn() {

	int32_t con = false;
	std::list<std::unique_ptr<TestRun>> threads;

	static const size_t count = 10;
	for (size_t i = 0; i < count; ++i) {
		threads.push_back(std::make_unique<TestRun>(con));
		if (!threads.back()->start(5, 1024)) {
			ESP_LOGE(TAG, "%s: not Started", __FUNCTION__);
			threads.pop_back();
			return false;
		}
	}

	if (con != count) {
		ESP_LOGE(TAG, "%s: not created %d %d", __FUNCTION__, con, count);
		return false;
	}
	vTaskDelay(pdMS_TO_TICKS(2));

	while (!threads.empty()) {
		threads.front()->join();
		threads.pop_front();
	}
	ESP_LOGI(TAG, "%s: PASS", __FUNCTION__);

	return true;
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))
#endif


bool Test_Runnable() {
	static const auto tests = {
		base,
		stop,
		kill,
		cancel,
		spawn,
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
		return false;
	}
	return true;
}
