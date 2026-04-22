#pragma once

#include <uuid.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace coupecad::core {

// Базовый wrapper. Не используется напрямую; используются строго
// типизированные алиасы (PanelId, CabinetId и т.п.) ниже через
// explicit-наследование.
template <class Tag>
class Id {
public:
    Id() = default;
    explicit Id(uuids::uuid u) noexcept : u_(u) {}

    static Id from_string(std::string_view s) {
        if (auto parsed = uuids::uuid::from_string(s)) {
            return Id{*parsed};
        }
        return Id{};   // invalid; callers should validate
    }

    bool is_valid() const noexcept { return !u_.is_nil(); }
    const uuids::uuid& raw() const noexcept { return u_; }
    std::string to_string() const { return uuids::to_string(u_); }

    // stduuid даёт ==, !=, <, но не <=> — поэтому пишем вручную.
    bool operator==(const Id& other) const noexcept { return u_ == other.u_; }
    bool operator!=(const Id& other) const noexcept { return u_ != other.u_; }
    bool operator<(const Id& other) const noexcept { return u_ < other.u_; }

private:
    uuids::uuid u_;
};

}  // namespace coupecad::core

// Hash support — нужно для использования Id в std::unordered_map.
namespace std {
template <class Tag>
struct hash<coupecad::core::Id<Tag>> {
    std::size_t operator()(const coupecad::core::Id<Tag>& id) const noexcept {
        return std::hash<uuids::uuid>{}(id.raw());
    }
};
}  // namespace std

namespace coupecad::core {

// Tag-структуры для Id<Tag>. Каждый Tag даёт независимый тип.
struct CabinetIdTag {};
struct PanelIdTag {};
struct MaterialIdTag {};
struct HardwareItemIdTag {};

using CabinetId      = Id<CabinetIdTag>;
using PanelId        = Id<PanelIdTag>;
using MaterialId     = Id<MaterialIdTag>;
using HardwareItemId = Id<HardwareItemIdTag>;

// HardwareRef — НЕ UUID, а строковый ключ карточки в каталоге
// (например "hinge.generic.straight"). Тоже strong-typed для безопасности.
class HardwareRef {
public:
    HardwareRef() = default;
    explicit HardwareRef(std::string s) noexcept : s_(std::move(s)) {}
    const std::string& value() const noexcept { return s_; }
    bool operator==(const HardwareRef&) const = default;
    auto operator<=>(const HardwareRef&) const = default;

private:
    std::string s_;
};

// Интерфейс генератора UUID. Real implementation использует random;
// тесты используют seeded для воспроизводимости.
class UuidGenerator {
public:
    virtual ~UuidGenerator() = default;
    virtual uuids::uuid next() = 0;

    template <class Tag>
    Id<Tag> next_id() {
        return Id<Tag>{next()};
    }
};

// Production-default: cryptographically random.
std::unique_ptr<UuidGenerator> make_random_uuid_generator();

// Seeded: воспроизводимая последовательность для тестов.
std::unique_ptr<UuidGenerator> make_seeded_uuid_generator(std::uint64_t seed);

}  // namespace coupecad::core

namespace std {
template <>
struct hash<coupecad::core::HardwareRef> {
    std::size_t operator()(const coupecad::core::HardwareRef& r) const noexcept {
        return std::hash<std::string>{}(r.value());
    }
};
}  // namespace std
