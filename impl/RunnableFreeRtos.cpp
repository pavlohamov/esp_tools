/*
 * RunnableFreeRtos.cpp
 *
 *  Created on: 31 Jul 2026
 *	  Author: pavloha
 */


#include "Runnable.hpp"


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"

#include <type_traits>
#include <stdint.h>


#ifdef CONFIG_SPIRAM
#define STACK_FLAGS_SPIRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
#define STACK_FLAGS (MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT)
#endif


class Runnable::Impl {
public:
	explicit Impl(Runnable& owner): owner_(owner) {}
	~Impl() {
		if (state_ != State::Idle)
			join();

		if (tid_) {
			vTaskDelete(tid_);
			tid_ = nullptr;
		}

		cleanup();
	}

	bool start(int prio, size_t stack_size, bool inpsram) noexcept {
		State expected = State::Idle;
		if (!state_.compare_exchange_strong(expected, State::Starting))
			return false;

		cleanup();

		stack_ = (StackType_t*)heap_caps_aligned_calloc(std::alignment_of<uint64_t>::value, 1, stack_size,
				MALLOC_CAP_8BIT | (inpsram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_DEFAULT));
		if (!stack_) {
			state_ = State::Idle;
			return false;
		}

		thread_ = (StaticTask_t*)heap_caps_aligned_calloc(sizeof(StaticTask_t*), 1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
		if (!thread_) {
			free(stack_);
			state_ = State::Idle;
			return false;
		}

		tid_ = xTaskCreateStatic(&Impl::taskTrampoline, "default", stack_size, this, prio, (StackType_t*)stack_, thread_);
		if (tid_)
			return true;

		cleanup();
		state_ = State::Idle;
		return false;
	}

	bool stop() noexcept {
		State expected = State::Running;
		return state_.compare_exchange_strong(expected, State::Stopping);
	}

	void join() noexcept {
		TaskHandle_t tid = tid_.exchange(nullptr);
		if (!tid)
			return;

		state_ = State::Stopping;
		while (state_ == State::Stopping) {
			// todo: need to constantly wake thread
			vTaskDelay(pdMS_TO_TICKS(1));
		}

		vTaskDelete(tid);
	}

	void cancel() noexcept {
		TaskHandle_t tid = tid_.exchange(nullptr);
		if (!tid)
			return;

		vTaskDelete(tid);
		state_ = State::Idle;
	}

	bool running() const noexcept { return state_ != State::Idle; }
	Runnable::State state() const noexcept { return state_; }
	const char* name() const noexcept { return tid_ ? pcTaskGetName(tid_) : "name?"; }

private:
	static void taskTrampoline(void* self) {
		auto* impl = static_cast<Impl*>(self);
		impl->state_ = State::Running;
		impl->owner_.run();
		impl->state_ = State::Idle;
		vTaskDelay(portMAX_DELAY);
	}

	void cleanup() {
		if (stack_) {
			free(stack_);
			stack_ = nullptr;
		}

		if (thread_) {
			free(thread_);
			thread_ = nullptr;
		}
	}

	Runnable& owner_;
	TaskHandle_t handle_ = nullptr;
	std::atomic<Runnable::State> state_{Runnable::State::Idle};

	StackType_t* stack_ = nullptr;
	StaticTask_t* thread_ = nullptr;
	std::atomic<TaskHandle_t> tid_{nullptr};
};

Runnable::Runnable(const char* name) : impl_(std::make_unique<Impl>(*this)) {}
Runnable::~Runnable() = default;

bool Runnable::start(int prio, size_t stack_size, bool inpsram) noexcept { return impl_->start(prio, stack_size, inpsram); }
bool Runnable::stop() noexcept { return impl_->stop(); }
void Runnable::join() noexcept { impl_->join(); }
void Runnable::cancel() noexcept { impl_->cancel(); }

bool Runnable::running() const noexcept { return impl_->running(); }
Runnable::State Runnable::state() const noexcept { return impl_->state(); }
const char* Runnable::name() const noexcept { return impl_->name(); }
