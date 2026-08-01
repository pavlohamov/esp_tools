/*
 * MutexFreeRtos.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "NonCopyable.hpp"
#include "NonMoveable.hpp"

#include <memory>

class MutexFrBase: public NonCopyable, public NonMoveable {
protected:
	MutexFrBase(SemaphoreHandle_t han) : handle_(han) {}
public:
	~MutexFrBase() {
		vSemaphoreDelete(handle_);
	}

protected:
	SemaphoreHandle_t handle_;
};

class MutexFr: public MutexFrBase {
public:
	MutexFr(): MutexFrBase(xSemaphoreCreateMutex()) {
	}

	void lock() noexcept {
		xSemaphoreTake(handle_, portMAX_DELAY);
	}

	void unlock() noexcept {
		xSemaphoreGive(handle_);
	}

	bool try_lock() noexcept {
		return xSemaphoreTake(handle_, 0) == pdTRUE;
	}
};

class MutexRecursiveFr: public MutexFrBase {
public:
	MutexRecursiveFr(): MutexFrBase(xSemaphoreCreateRecursiveMutex()) {
	}

	void lock() noexcept {
		xSemaphoreTakeRecursive(handle_, portMAX_DELAY);
	}

	void unlock() noexcept {
		xSemaphoreGiveRecursive(handle_);
	}

	bool try_lock() noexcept {
		return xSemaphoreTakeRecursive(handle_, 0) == pdTRUE;
	}
};
