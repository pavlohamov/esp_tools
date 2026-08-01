/*
 * Singleton.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */

#pragma once

#include <type_traits>
#include "NonCopyable.hpp"
#include "NonMoveable.hpp"

template <typename Derived>
class Singleton: public NonCopyable, NonMoveable {
public:
    static Derived& instance() noexcept(std::is_nothrow_default_constructible<Derived>::value)
    {
#ifndef SINGLETON_INJECT_ABSTRACT_CLASS
        static Derived instance;
#else
        struct Dummy final : Derived {
            void ProhibitConstructFromDerived() const noexcept override { }
        };
        static Dummy instance;
#endif // SINGLETON_INJECT_ABSTRACT_CLASS

        return instance;
    }

protected:
    Singleton() = default;
#ifndef SINGLETON_INJECT_ABSTRACT_CLASS
    ~Singleton() = default;
#else
    virtual ~Singleton() = default;

private:
    virtual void ProhibitConstructFromDerived() const noexcept = 0;
#endif // SINGLETON_INJECT_ABSTRACT_CLASS
};

