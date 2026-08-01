/*
 * NonCopyable.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#pragma once

#include <type_traits>

namespace NonCopyable_ { // protection from unintended ADL

class NonCopyable {
protected:
	constexpr NonCopyable() = default;
	~NonCopyable() = default;

	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};


class Derived : public NonCopyable {
public:
	Derived() = default;
};

static_assert(!std::is_copy_constructible_v<Derived>, "Copy-constructible");
static_assert(!std::is_copy_assignable_v<Derived>, "Copy-assignable");

};
using NonCopyable = NonCopyable_::NonCopyable;
