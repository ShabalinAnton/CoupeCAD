#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/io/project_serializer.h"
#include "coupecad/core/units.h"

#include <nlohmann/json.hpp>

namespace coupecad::core {

class Project;

// JSON-реализация ProjectSerializer поверх nlohmann::json.
// Сериализация детерминированная: массивы сортируются по id/ref,
// ключи объектов в фиксированном порядке.
class JsonProjectSerializer : public ProjectSerializer {
public:
    const char* content_encoding() const noexcept override { return "json"; }

    std::vector<std::uint8_t> serialize(const Project& project) const override;
    Project deserialize(const std::vector<std::uint8_t>& bytes) const override;
};

// Внутренние converter'ы (header-visible для unit-тестов отдельных типов).
// Все to_json / from_json работают с nlohmann::json напрямую.
namespace detail {

nlohmann::json to_json_millimeters(Millimeters m);
Millimeters from_json_millimeters(const nlohmann::json& j);

nlohmann::json to_json_dimensions(const Dimensions& d);
Dimensions from_json_dimensions(const nlohmann::json& j);

nlohmann::json to_json_vec3(const Vec3& v);
Vec3 from_json_vec3(const nlohmann::json& j);

nlohmann::json to_json_quat(const Quat& q);
Quat from_json_quat(const nlohmann::json& j);

nlohmann::json to_json_money(const Money& m);
Money from_json_money(const nlohmann::json& j);

nlohmann::json to_json_rgba(const RGBA& c);
RGBA from_json_rgba(const nlohmann::json& j);

// IDs — как hex-строки.
template <class Tag>
nlohmann::json to_json_id(const Id<Tag>& id);
template <class Tag>
Id<Tag> from_json_id(const nlohmann::json& j);

}  // namespace detail

}  // namespace coupecad::core
