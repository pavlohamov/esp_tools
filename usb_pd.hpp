/*
 * usb_pd.hpp
 *
 *  Created on: 26 Aug 2026
 *      Author: pavloha
 */

#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace usbpd {

namespace detail {
    struct volt_tag;
    struct amp_tag;
    struct watt_tag;
}

template <typename Tag>
class quantity {
public:
    constexpr quantity() = default;
    constexpr explicit quantity(uint32_t milli) : milli_(milli) {}

    constexpr uint32_t milli() const { return milli_; }

    friend constexpr auto operator<=>(quantity, quantity) = default;
    friend constexpr bool operator==(quantity, quantity)  = default;

    friend constexpr quantity operator+(quantity a, quantity b) { return quantity{ a.milli_ + b.milli_ }; }
    friend constexpr quantity min(quantity a, quantity b) { return a < b ? a : b; }
    friend constexpr quantity max(quantity a, quantity b) { return a < b ? b : a; }

private:
    uint32_t milli_ = 0;
};

using millivolts = quantity<detail::volt_tag>;
using milliamps  = quantity<detail::amp_tag>;
using milliwatts = quantity<detail::watt_tag>;

constexpr milliwatts operator*(millivolts v, milliamps i) {
    return milliwatts { static_cast<uint32_t>(static_cast<uint64_t>(v.milli()) * i.milli() / 1000u) };
}
constexpr milliwatts operator*(milliamps i, millivolts v) { return v * i; }


constexpr milliamps current_from(milliwatts p, millivolts v) {
    if (v.milli() == 0)
        return milliamps{0};
    return milliamps { static_cast<uint32_t>(static_cast<uint64_t>(p.milli()) * 1000u / v.milli()) };
}

inline namespace literals {
    constexpr millivolts operator""_mV(unsigned long long x) { return millivolts{static_cast<uint32_t>(x)}; }
    constexpr millivolts operator""_V(unsigned long long x)  { return millivolts{static_cast<uint32_t>(x * 1000)}; }
    constexpr milliamps  operator""_mA(unsigned long long x) { return milliamps{static_cast<uint32_t>(x)}; }
    constexpr milliamps  operator""_A(unsigned long long x)  { return milliamps{static_cast<uint32_t>(x * 1000)}; }
    constexpr milliwatts operator""_mW(unsigned long long x) { return milliwatts{static_cast<uint32_t>(x)}; }
    constexpr milliwatts operator""_W(unsigned long long x)  { return milliwatts{static_cast<uint32_t>(x * 1000)}; }
} // namespace literals


enum class pdo_kind : uint8_t {
    fixed,     // Fixed Supply
    battery,   // Battery  (power, not current)
    variable,  // Variable Supply (non-battery)
    spr_pps,   // APDO: SPR Programmable Power Supply
    epr_avs,   // APDO: EPR Adjustable Voltage Supply (set PDP)
    spr_avs,   // APDO: SPR Adjustable Voltage Supply (PD 3.2)
    unknown,
};

constexpr const char* to_string(pdo_kind k) {
    switch (k) {
        case pdo_kind::fixed:    return "Fixed";
        case pdo_kind::battery:  return "Battery";
        case pdo_kind::variable: return "Variable";
        case pdo_kind::spr_pps:  return "SPR-PPS";
        case pdo_kind::epr_avs:  return "EPR-AVS";
        case pdo_kind::spr_avs:  return "SPR-AVS";
        default:                 return "Unknown";
    }
}


class pdo {
public:
    constexpr pdo() = default;
    constexpr explicit pdo(uint32_t raw) : raw_(raw) {}

    constexpr uint32_t raw() const { return raw_; }
    constexpr bool valid() const { return raw_ != 0 && kind() != pdo_kind::unknown; }
    constexpr explicit operator bool() const { return valid(); }

    constexpr pdo_kind kind() const {
        switch (bits(31, 30)) {
            case 0b00: return pdo_kind::fixed;
            case 0b01: return pdo_kind::battery;
            case 0b10: return pdo_kind::variable;
            default:
                switch (bits(29, 28)) {
                    case 0b00: return pdo_kind::spr_pps;
                    case 0b01: return pdo_kind::epr_avs;
                    case 0b10: return pdo_kind::spr_avs;
                    default:   return pdo_kind::unknown;
                }
        }
    }

    constexpr bool is_programmable() const {
        const auto k = kind();
        return k == pdo_kind::spr_pps || k == pdo_kind::epr_avs || k == pdo_kind::spr_avs;
    }

    constexpr millivolts max_voltage() const {
        switch (kind()) {
            case pdo_kind::fixed:    return millivolts{bits(19, 10) * 50};
            case pdo_kind::battery:
            case pdo_kind::variable: return millivolts{bits(29, 20) * 50};
            case pdo_kind::spr_pps:  return millivolts{bits(24, 17) * 100};
            case pdo_kind::epr_avs:  return millivolts{bits(25, 17) * 100};
            case pdo_kind::spr_avs:  return millivolts{20000};  // SPR AVS fixed 9–20 V
            default:                 return millivolts{};
        }
    }

    constexpr millivolts min_voltage() const {
        switch (kind()) {
            case pdo_kind::fixed:    return max_voltage();
            case pdo_kind::battery:
            case pdo_kind::variable: return millivolts{bits(19, 10) * 50};
            case pdo_kind::spr_pps:
            case pdo_kind::epr_avs:  return millivolts{bits(15, 8) * 100};
            case pdo_kind::spr_avs:  return millivolts{9000};
            default:                 return millivolts{};
        }
    }

    constexpr std::optional<milliamps> max_current() const {
        switch (kind()) {
            case pdo_kind::fixed:
            case pdo_kind::variable: return milliamps{bits(9, 0) * 10};
            case pdo_kind::spr_pps:  return milliamps{bits(6, 0) * 50};
            case pdo_kind::spr_avs:  return max(milliamps{bits(9, 0) * 10},     // 9–15 V
                                                milliamps{bits(19, 10) * 10});  // 15–20 V
            case pdo_kind::battery:
            case pdo_kind::epr_avs:  return std::nullopt;
            default:                 return std::nullopt;
        }
    }

    constexpr milliwatts max_power() const {
        switch (kind()) {
            case pdo_kind::battery: return milliwatts{bits(9, 0) * 250};
            case pdo_kind::epr_avs: return milliwatts{bits(9, 0) * 1000};  // PDP в ваттах
            default:                return power_at(max_voltage());
        }
    }

    constexpr bool supports(millivolts v) const {
        if (!valid())
            return false;
        if (kind() == pdo_kind::fixed)
            return v == max_voltage();
        return v >= min_voltage() && v <= max_voltage();
    }

    constexpr milliamps current_at(millivolts v) const {
        if (!supports(v))
            return milliamps{};
        switch (kind()) {
            case pdo_kind::fixed:
            case pdo_kind::variable: return milliamps{bits(9, 0) * 10};
            case pdo_kind::spr_pps:  return milliamps{bits(6, 0) * 50};
            case pdo_kind::battery:  return current_from(max_power(), v);
            case pdo_kind::epr_avs:  return min(current_from(max_power(), v), milliamps{5000});
            case pdo_kind::spr_avs:  return v <= millivolts{15000} ? milliamps{bits(9, 0) * 10}
                                                                  : milliamps{bits(19, 10) * 10};
            default:                 return milliamps{};
        }
    }

    constexpr milliwatts power_at(millivolts v) const { return v * current_at(v); }

    constexpr std::optional<millivolts> best_voltage_in(millivolts lo, millivolts hi) const {
        if (!valid())
            return std::nullopt;
        if (kind() == pdo_kind::fixed) {
            const auto v = max_voltage();
            return (v >= lo && v <= hi) ? std::optional{v} : std::nullopt;
        }
        const auto v = min(max_voltage(), hi);
        return (v >= lo && v >= min_voltage()) ? std::optional{v} : std::nullopt;
    }

    constexpr bool dual_role_power() const       { return bits(29, 29) != 0; }
    constexpr bool usb_suspend_supported() const { return bits(28, 28) != 0; }
    constexpr bool unconstrained_power() const   { return bits(27, 27) != 0; }
    constexpr bool usb_comms_capable() const     { return bits(26, 26) != 0; }
    constexpr bool dual_role_data() const        { return bits(25, 25) != 0; }
    constexpr bool epr_mode_capable() const      { return bits(23, 23) != 0; }

    friend constexpr bool operator==(pdo, pdo) = default;

private:
    constexpr uint32_t bits(unsigned hi, unsigned lo) const {
        return (raw_ >> lo) & ((1u << (hi - lo + 1)) - 1u);
    }
    uint32_t raw_ = 0;
};

static_assert(sizeof(pdo) == 4);

class source_capabilities {
public:
    static constexpr std::size_t max_spr_pdos = 7;
    static constexpr std::size_t capacity     = 11;  // SPR(7) + EPR(4)

    struct selection {
        std::size_t object_position;
        pdo         source;
        millivolts  voltage;
        milliamps   current;

        constexpr milliwatts power() const { return voltage * current; }
        constexpr explicit operator bool() const { return object_position != 0; }
    };

    constexpr void clear() { count_ = 0; }

    constexpr bool add(pdo p) {
        if (count_ >= capacity)
            return false;
        pdos_[count_++] = p;
        return true;
    }

    constexpr std::size_t size() const { return count_; }
    constexpr bool empty() const { return count_ == 0; }

    constexpr const pdo& operator[](std::size_t i) const { return pdos_[i]; }
    constexpr const pdo* begin() const { return pdos_.data(); }
    constexpr const pdo* end() const { return pdos_.data() + count_; }

    constexpr selection best_power(millivolts vmin, millivolts vmax, milliamps imin = milliamps{}) const {
        selection best{};
        for (std::size_t i = 0; i < count_; ++i) {
            const auto v = pdos_[i].best_voltage_in(vmin, vmax);
            if (!v)
                continue;
            const auto a = pdos_[i].current_at(*v);
            if (a < imin)
                continue;
            if (!best || (*v * a) > best.power())
                best = {i + 1, pdos_[i], *v, a};
        }
        return best;
    }

    constexpr selection at_voltage(millivolts v, milliamps imin = milliamps{}) const {
        selection best{};
        for (std::size_t i = 0; i < count_; ++i) {
            if (!pdos_[i].supports(v))
                continue;
            const auto a = pdos_[i].current_at(v);
            if (a < imin)
                continue;
            if (!best || a > best.current)
                best = {i + 1, pdos_[i], v, a};
        }
        return best;
    }

private:
    std::array<pdo, capacity> pdos_{};
    std::size_t count_ = 0;
};


}
