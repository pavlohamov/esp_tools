/*
 * Buttons.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#pragma once

#include <vector>
#include <cstdint>
#include <memory>

#include "Singleton.hpp"

namespace Buttons {

enum {
	PRESS,
	FIRST_REPEAT,
	REPEAT,
	RELEASE,
};

typedef void (*onButtonEvt_f) (int btn, int evt, void *arg);


class Button;

class Wrapper final: public Singleton<Wrapper> {
private:
	friend class Singleton<Wrapper>;
public:

	std::shared_ptr<Button> add(int gpio, bool actlo, int signal, onButtonEvt_f cb, void *arg);
	bool del(std::shared_ptr<Button>& b);

//   0 - released
//   1 - pressed
// < 0 - error
	int state(int btn) const;
// pressed for ms
	int pressed_for(int btn) const;
// ms since released
	int released_for(int btn) const;

	int debounce_ms(int btn) const;
	int first_repeat_ms(int btn) const;
	int next_repeat_ms(int btn) const;

private:
	std::vector<std::shared_ptr<Button>> _buttons;
};

};
