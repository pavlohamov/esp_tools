/*
 * NonMoveable.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#pragma once

#include <type_traits>

namespace NonMoveable_ { // protection from unintended ADL

class NonMoveable {
protected:
	constexpr NonMoveable() = default;
	~NonMoveable() = default;

	NonMoveable(NonMoveable&&) = delete;
	NonMoveable& operator=(NonMoveable&&) = delete;
};

class Derived : public NonMoveable {
public:
	Derived() = default;
};

static_assert(!std::is_move_constructible_v<Derived>, "Move-constructible");
static_assert(!std::is_move_assignable_v<Derived>, "Move-assignable");

};
using NonMoveable = NonMoveable_::NonMoveable;
