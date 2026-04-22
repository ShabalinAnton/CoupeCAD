#include "coupecad/core/io/json_project_serializer.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/id.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

namespace detail {

nlohmann::json to_json_millimeters(Millimeters m) { return m.value(); }

Millimeters from_json_millimeters(const nlohmann::json& j) {
    if (!j.is_number_integer()) {
        throw InvalidData{"json.millimeters_not_int",
                          "Expected integer for Millimeters"};
    }
    return Millimeters{j.get<std::int32_t>()};
}

nlohmann::json to_json_dimensions(const Dimensions& d) {
    return nlohmann::json{
        {"width", d.width.value()},
        {"depth", d.depth.value()},
        {"height", d.height.value()},
    };
}

Dimensions from_json_dimensions(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw InvalidData{"json.dimensions_not_object",
                          "Dimensions must be an object"};
    }
    return Dimensions{
        .width = from_json_millimeters(j.at("width")),
        .depth = from_json_millimeters(j.at("depth")),
        .height = from_json_millimeters(j.at("height")),
    };
}

nlohmann::json to_json_vec3(const Vec3& v) {
    return nlohmann::json::array({v.x.value(), v.y.value(), v.z.value()});
}

Vec3 from_json_vec3(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 3) {
        throw InvalidData{"json.vec3_shape",
                          "Vec3 must be array of 3 integers"};
    }
    return Vec3{from_json_millimeters(j[0]),
                from_json_millimeters(j[1]),
                from_json_millimeters(j[2])};
}

nlohmann::json to_json_quat(const Quat& q) {
    return nlohmann::json::array({q.w, q.x, q.y, q.z});
}

Quat from_json_quat(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 4) {
        throw InvalidData{"json.quat_shape",
                          "Quat must be array of 4 doubles"};
    }
    return Quat{j[0].get<double>(), j[1].get<double>(),
                j[2].get<double>(), j[3].get<double>()};
}

nlohmann::json to_json_money(const Money& m) { return m.minor_units; }

Money from_json_money(const nlohmann::json& j) {
    if (!j.is_number_integer()) {
        throw InvalidData{"json.money_not_int", "Money must be integer"};
    }
    return Money{.minor_units = j.get<std::int64_t>()};
}

nlohmann::json to_json_rgba(const RGBA& c) {
    return nlohmann::json::array({c.r, c.g, c.b, c.a});
}

RGBA from_json_rgba(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 4) {
        throw InvalidData{"json.rgba_shape",
                          "RGBA must be array of 4 uint8s"};
    }
    return RGBA{j[0].get<std::uint8_t>(), j[1].get<std::uint8_t>(),
                j[2].get<std::uint8_t>(), j[3].get<std::uint8_t>()};
}

template <class Tag>
nlohmann::json to_json_id(const Id<Tag>& id) {
    return id.is_valid() ? nlohmann::json(id.to_string()) : nlohmann::json(nullptr);
}

template <class Tag>
Id<Tag> from_json_id(const nlohmann::json& j) {
    if (j.is_null()) return Id<Tag>{};
    if (!j.is_string()) {
        throw InvalidData{"json.id_not_string",
                          "Id must be canonical UUID hex string or null"};
    }
    auto parsed = Id<Tag>::from_string(j.get<std::string>());
    if (!parsed.is_valid() && !j.get<std::string>().empty()) {
        throw InvalidData{"json.id_parse_failed",
                          "Id string is not a valid UUID"};
    }
    return parsed;
}

// Явные инстанциации для всех id-типов, которые будут сериализованы.
template nlohmann::json to_json_id(const CabinetId&);
template nlohmann::json to_json_id(const PanelId&);
template nlohmann::json to_json_id(const MaterialId&);
template nlohmann::json to_json_id(const HardwareItemId&);
template CabinetId from_json_id(const nlohmann::json&);
template PanelId from_json_id(const nlohmann::json&);
template MaterialId from_json_id(const nlohmann::json&);
template HardwareItemId from_json_id(const nlohmann::json&);

}  // namespace detail

// Заглушки для будущих Tasks 3-5.
std::vector<std::uint8_t> JsonProjectSerializer::serialize(const Project&) const {
    throw InvalidData{"json.not_implemented_yet",
                      "JsonProjectSerializer::serialize pending Task 5"};
}

Project JsonProjectSerializer::deserialize(const std::vector<std::uint8_t>&) const {
    throw InvalidData{"json.not_implemented_yet",
                      "JsonProjectSerializer::deserialize pending Task 5"};
}

}  // namespace coupecad::core
