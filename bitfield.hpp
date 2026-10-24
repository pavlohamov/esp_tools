/*
 * bitfield.hpp
 *
 *  Created on: 27 Sept 2026
 *      Author: pavloha
 */

#include <concepts>
#include <expected>
#include <limits>
#include <system_error>

namespace BitField {

template<unsigned hi, unsigned lo = hi> struct at_t {};
template<unsigned hi, unsigned lo = hi> inline constexpr at_t<hi, lo> at{};

template<std::unsigned_integral T, unsigned hi, unsigned lo = hi>
struct Field {
    static_assert(lo <= hi && hi < std::numeric_limits<T>::digits);
    static constexpr unsigned width = hi - lo + 1;
    static constexpr T low_mask = width == std::numeric_limits<T>::digits
        ? std::numeric_limits<T>::max() : static_cast<T>((T{1} << width) - 1);
    static constexpr T mask = static_cast<T>(low_mask << lo);

    static constexpr T extract(T v)      { return static_cast<T>((v >> lo) & low_mask); }
    static constexpr T insert(T v, T f)  { return static_cast<T>((v & ~mask) | ((f & low_mask) << lo)); }
};

template<typename R>
concept Register = requires(R r, typename R::value_type v) {
    { r.read() }   -> std::same_as<std::expected<typename R::value_type, std::error_code>>;
    { r.write(v) } -> std::same_as<std::expected<void, std::error_code>>;
};

template<Register R, unsigned hi, unsigned lo = hi>
class Bits {
    using T = R::value_type;
    using F = Field<T, hi, lo>;
    R& reg_;
public:
    constexpr explicit Bits(R& r) : reg_(r) {}
    constexpr Bits(R& r, at_t<hi, lo>) : reg_(r) {}

    std::expected<T, std::error_code> get() const {
        return reg_.read().transform(F::extract);
    }
    std::expected<void, std::error_code> set(T f) {
        if (f > F::low_mask)
            return std::unexpected(std::make_error_code((std::errc::invalid_argument)));
        return reg_.read().and_then([&](T v) { return reg_.write(F::insert(v, f)); });
    }
};

}; // namespace BitField