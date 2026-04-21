#pragma once

#include <cstdint>
#include <compare>
#include <cstddef>

namespace coupecad::core {

// Strong-typed целочисленный миллиметр (см. spec §2.3).
// Значения хранятся как int32_t — диапазон ±2.1·10^9 мм заведомо
// больше любых разумных размеров мебели.
class Millimeters {
public:
    constexpr Millimeters() = default;
    constexpr explicit Millimeters(std::int32_t value) noexcept : v_(value) {}

    constexpr std::int32_t value() const noexcept { return v_; }

    constexpr auto operator<=>(const Millimeters&) const = default;

    constexpr Millimeters operator+(Millimeters rhs) const noexcept {
        return Millimeters{v_ + rhs.v_};
    }
    constexpr Millimeters operator-(Millimeters rhs) const noexcept {
        return Millimeters{v_ - rhs.v_};
    }
    constexpr Millimeters& operator+=(Millimeters rhs) noexcept {
        v_ += rhs.v_;
        return *this;
    }
    constexpr Millimeters& operator-=(Millimeters rhs) noexcept {
        v_ -= rhs.v_;
        return *this;
    }
    constexpr Millimeters operator-() const noexcept {
        return Millimeters{-v_};
    }
    constexpr Millimeters operator*(std::int32_t k) const noexcept {
        return Millimeters{v_ * k};
    }

private:
    std::int32_t v_ = 0;
};

constexpr Millimeters operator*(std::int32_t k, Millimeters m) noexcept {
    return m * k;
}

// Удобный литерал: 1500_mm
constexpr Millimeters operator""_mm(unsigned long long v) {
    return Millimeters{static_cast<std::int32_t>(v)};
}

// Внешние габариты шкафа (см. spec §2.1, §2.3).
struct Dimensions {
    Millimeters width{};
    Millimeters depth{};
    Millimeters height{};

    constexpr bool operator==(const Dimensions&) const = default;
};

// 3D-вектор для local_position и Custom-панелей. Целочисленный мм.
struct Vec3 {
    Millimeters x{};
    Millimeters y{};
    Millimeters z{};

    constexpr bool operator==(const Vec3&) const = default;
};

// Кватернион для ориентации (Custom + HardwareItem). w-first, double.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr bool operator==(const Quat&) const = default;
    static constexpr Quat identity() noexcept { return {1.0, 0.0, 0.0, 0.0}; }
};

// Деньги — целочисленный «мини-юнит» (копейки/центы), валюта отдельно.
// В v1 валюта — фиксированная RUB; полноценная многовалютность позже.
struct Money {
    std::int64_t minor_units = 0;     // копейки
    constexpr bool operator==(const Money&) const = default;
};

// Цвет в формате sRGB + альфа.
struct RGBA {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
    constexpr bool operator==(const RGBA&) const = default;
};

}  // namespace coupecad::core
