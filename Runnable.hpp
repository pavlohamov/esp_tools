/*
 * Runnable.hpp
 *
 *  Created on: 31 Jul 2026
 *      Author: pavloha
 */


#pragma once

#include "NonCopyable.hpp"
#include "NonMoveable.hpp"

#include <atomic>
#include <memory>
#include <stddef.h>

class Runnable : public NonCopyable, public NonMoveable {
public:
    enum class State {
        Idle,
        Starting,
        Running,
        Stopping,
    };

    Runnable(const char* name = "thread") noexcept;
    virtual ~Runnable() noexcept;

    [[nodiscard]] bool start() noexcept;
    [[nodiscard]] bool start(int prio, size_t stackSize, bool inpsram = false) noexcept;
    [[nodiscard]] bool stop() noexcept;
    void join() noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

protected:
    virtual void run() noexcept = 0;

private:
    std::atomic<State> state_{State::Idle};

    class Impl;
    std::unique_ptr<Impl> impl_;
};
