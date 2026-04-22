#pragma once

#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/io/project_serializer.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
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

// --- Material ---
nlohmann::json to_json_material(const Material& m);
Material from_json_material(const nlohmann::json& j);

// --- EdgeBanding ---
nlohmann::json to_json_edge_banding(const EdgeBanding& eb);
EdgeBanding from_json_edge_banding(const nlohmann::json& j);

nlohmann::json to_json_panel_edge_banding(const PanelEdgeBanding& eb);
PanelEdgeBanding from_json_panel_edge_banding(const nlohmann::json& j);

// --- HardwareSpec ---
nlohmann::json to_json_hardware_spec(const HardwareSpec& s);
HardwareSpec from_json_hardware_spec(const nlohmann::json& j);

// --- HardwareItem / PanelAttachment ---
nlohmann::json to_json_panel_attachment(const PanelAttachment& a);
PanelAttachment from_json_panel_attachment(const nlohmann::json& j);

nlohmann::json to_json_hardware_item(const HardwareItem& h);
HardwareItem from_json_hardware_item(const nlohmann::json& j);

// Вспомогательные перечисления <-> строки.
const char* material_kind_to_str(MaterialKind k);
MaterialKind material_kind_from_str(const std::string& s);
const char* hardware_kind_to_str(HardwareKind k);
HardwareKind hardware_kind_from_str(const std::string& s);

nlohmann::json to_json_role_params(const RoleParams& rp);
RoleParams from_json_role_params(const nlohmann::json& j);

nlohmann::json to_json_panel(const Panel& p);
Panel from_json_panel(const nlohmann::json& j);

const char* panel_role_to_str(PanelRole r);
PanelRole panel_role_from_str(const std::string& s);

const char* grain_direction_to_str(GrainDirection g);
GrainDirection grain_direction_from_str(const std::string& s);

const char* hinge_side_to_str(HingeSide h);
HingeSide hinge_side_from_str(const std::string& s);

}  // namespace detail

}  // namespace coupecad::core
