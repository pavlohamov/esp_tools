/*
 * Buttons.cpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#include "Buttons.hpp"

#include <list>
#include <errno.h>

#include "esp_timer.h"
#include "driver/gpio.h"

#include "esp_log.h"
static const char *TAG = "Buttons";

using namespace Buttons;


#define TOUT_DEBOUNCE_US (25UL * 1000UL)

#define TOUT_FIRST_REPEAT_US (300UL * 1000UL)
#define TOUT_REPEAT_US (125UL * 1000UL)

enum {
	STT_RELEASED,
	STT_PRESSED,
	STT_REPEATED,
};

class Buttons::Button {
public:
	Button(gpio_num_t gpio, bool actlo, int btn, onButtonEvt_f cb, void *arg): _gpio(gpio), _actlo(actlo), _signal(btn), _cb(cb), _arg(arg), _state(STT_RELEASED), _pressedAt(0) {

		const esp_timer_create_args_t initer = {
			.callback = onTout,
			.arg = this,
			.dispatch_method = ESP_TIMER_TASK,
			.name = "gpio_cb",
			.skip_unhandled_events = false,
		};
		ESP_ERROR_CHECK(esp_timer_create(&initer, &_tim));

		const gpio_config_t cfg = {
			.pin_bit_mask = BIT64(_gpio),
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = actlo ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
			.pull_down_en = actlo ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
			.intr_type = GPIO_INTR_ANYEDGE,
		};
		ESP_ERROR_CHECK(gpio_config(&cfg));
		if (gpio_isr_handler_add(_gpio, onButtonCb, this)) {
			ESP_ERROR_CHECK(gpio_install_isr_service(0));
			ESP_ERROR_CHECK(gpio_isr_handler_add(_gpio, onButtonCb, this));
		}
		ESP_ERROR_CHECK(gpio_intr_enable(_gpio));
	}

	virtual ~Button() {
		ESP_ERROR_CHECK(gpio_intr_disable(_gpio));
		ESP_ERROR_CHECK(gpio_isr_handler_remove(_gpio));
		ESP_ERROR_CHECK(esp_timer_delete(_tim));
	}
private:
	void inline callback(int evt) {
		if (_cb)
			_cb(_signal, evt, _arg);
	}
private:
	const gpio_num_t _gpio;
	const bool _actlo;
	const int _signal;
	const onButtonEvt_f _cb;
	void *const _arg;
	int _state;
	esp_timer_handle_t _tim;
	int64_t _pressedAt;
	int64_t _releasedAt;

public:
	static void onButtonCb(void* arg) {
		Button *b = static_cast<Button*>(arg);
		if (gpio_get_level(b->_gpio) ^ b->_actlo) {
			esp_timer_stop(b->_tim);
			ESP_ERROR_CHECK(esp_timer_start_once(b->_tim, TOUT_DEBOUNCE_US));
		} else {
			ESP_ERROR_CHECK(esp_timer_stop(b->_tim));
			int was = b->_state;
			b->_state = STT_RELEASED;
			b->_pressedAt = 0;
			b->_releasedAt = esp_timer_get_time() / 1000UL;
			if (was)
				b->callback(RELEASE);
		}
	}

	static void onTout(void* arg) {
		Button *b = static_cast<Button*>(arg);
		switch (b->_state) {
			case STT_RELEASED:
				b->_pressedAt = esp_timer_get_time() / 1000UL;
				b->_releasedAt = 0;
				esp_timer_stop(b->_tim);
				ESP_ERROR_CHECK(esp_timer_start_once(b->_tim, TOUT_FIRST_REPEAT_US));
				b->callback(PRESS);
				b->_state = STT_PRESSED;
				break;
			case STT_PRESSED:
				b->_state = STT_REPEATED;
				esp_timer_stop(b->_tim);
				ESP_ERROR_CHECK(esp_timer_start_periodic(b->_tim, TOUT_REPEAT_US));
				b->callback(FIRST_REPEAT);
				break;
			case STT_REPEATED:
				b->callback(REPEAT);
				break;
		}
	}

	int state() {
		return _state != STT_RELEASED;
	}
	int pressedFor() {
		return _pressedAt ? esp_timer_get_time() / 1000UL - _pressedAt : 0;
	}
	int releasedFor() {
		return _releasedAt ? esp_timer_get_time() / 1000UL - _releasedAt : 0;
	}
};

std::shared_ptr<Button> Wrapper::add(int gpio, bool actlo, int signal, onButtonEvt_f cb, void *arg) {
	auto b = std::make_shared<Button>((gpio_num_t)gpio, actlo, signal, cb, arg);
	if (!b) {
		ESP_LOGE(TAG, "No MEM!");
		return NULL;
	}

	_buttons.push_back(b);
	return b;
}

bool Wrapper::del(std::shared_ptr<Button>& b) {
	auto it = std::find(_buttons.begin(), _buttons.end(), b);
	if (it == _buttons.end())
		return false;
	_buttons.erase(it);
	b.reset();
	return true;
}

//   0 - released
//   1 - pressed
// < 0 - error
int Wrapper::state(int btn) const {
	if ((size_t)btn >= _buttons.size())
		return -EINVAL;
	return _buttons[btn]->state();
}

int Wrapper::pressed_for(int btn) const {
	if ((size_t)btn >= _buttons.size())
		return -EINVAL;
	return _buttons[btn]->pressedFor();
}

int Wrapper::released_for(int btn) const {
	if ((size_t)btn >= _buttons.size())
		return -EINVAL;
	return _buttons[btn]->releasedFor();
}

int Wrapper::debounce_ms(int btn) const {
	return TOUT_DEBOUNCE_US / 1000;
}

int Wrapper::first_repeat_ms(int btn) const {
	return TOUT_FIRST_REPEAT_US / 1000;
}

int Wrapper::next_repeat_ms(int btn) const {
	return TOUT_REPEAT_US / 1000;
}

