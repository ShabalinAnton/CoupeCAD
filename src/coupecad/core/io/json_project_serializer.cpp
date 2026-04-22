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

const char* material_kind_to_str(MaterialKind k) {
    return material_kind_name(k);
}

MaterialKind material_kind_from_str(const std::string& s) {
    if (s == "ChipboardLaminated") return MaterialKind::ChipboardLaminated;
    if (s == "Mdf")               return MaterialKind::Mdf;
    if (s == "Hdf")               return MaterialKind::Hdf;
    if (s == "Plywood")           return MaterialKind::Plywood;
    if (s == "SolidWood")         return MaterialKind::SolidWood;
    if (s == "Glass")             return MaterialKind::Glass;
    if (s == "Metal")             return MaterialKind::Metal;
    if (s == "Other")             return MaterialKind::Other;
    throw InvalidData{"json.material_kind_unknown",
                      "unknown MaterialKind: " + s};
}

const char* hardware_kind_to_str(HardwareKind k) {
    return hardware_kind_name(k);
}

HardwareKind hardware_kind_from_str(const std::string& s) {
    if (s == "Hinge")        return HardwareKind::Hinge;
    if (s == "DrawerSlide")  return HardwareKind::DrawerSlide;
    if (s == "Handle")       return HardwareKind::Handle;
    if (s == "ShelfSupport") return HardwareKind::ShelfSupport;
    if (s == "Connector")    return HardwareKind::Connector;
    if (s == "GasLift")      return HardwareKind::GasLift;
    if (s == "Other")        return HardwareKind::Other;
    throw InvalidData{"json.hardware_kind_unknown",
                      "unknown HardwareKind: " + s};
}

nlohmann::json to_json_material(const Material& m) {
    nlohmann::json j;
    j["id"] = to_json_id(m.id);
    j["name"] = m.name;
    j["kind"] = material_kind_to_str(m.kind);
    j["default_thickness_mm"] = m.default_thickness.value();
    j["color_hint"] = to_json_rgba(m.color_hint);
    j["texture_ref"] = m.texture_ref ? nlohmann::json(*m.texture_ref) : nlohmann::json(nullptr);
    j["price_per_sqm"] = m.price_per_sqm ? to_json_money(*m.price_per_sqm) : nlohmann::json(nullptr);
    return j;
}

Material from_json_material(const nlohmann::json& j) {
    Material m;
    m.id = from_json_id<MaterialIdTag>(j.at("id"));
    m.name = j.at("name").get<std::string>();
    m.kind = material_kind_from_str(j.at("kind").get<std::string>());
    m.default_thickness = from_json_millimeters(j.at("default_thickness_mm"));
    m.color_hint = from_json_rgba(j.at("color_hint"));
    if (!j.at("texture_ref").is_null()) {
        m.texture_ref = j.at("texture_ref").get<std::string>();
    }
    if (!j.at("price_per_sqm").is_null()) {
        m.price_per_sqm = from_json_money(j.at("price_per_sqm"));
    }
    return m;
}

nlohmann::json to_json_edge_banding(const EdgeBanding& eb) {
    nlohmann::json j;
    j["material_id"] = to_json_id(eb.material_id);
    j["thickness_mm"] = eb.thickness.value();
    return j;
}

EdgeBanding from_json_edge_banding(const nlohmann::json& j) {
    EdgeBanding eb;
    eb.material_id = from_json_id<MaterialIdTag>(j.at("material_id"));
    eb.thickness = from_json_millimeters(j.at("thickness_mm"));
    return eb;
}

nlohmann::json to_json_panel_edge_banding(const PanelEdgeBanding& eb) {
    auto side = [](const std::optional<EdgeBanding>& s) -> nlohmann::json {
        return s ? to_json_edge_banding(*s) : nlohmann::json(nullptr);
    };
    return nlohmann::json{
        {"front", side(eb.front)},
        {"back", side(eb.back)},
        {"left", side(eb.left)},
        {"right", side(eb.right)},
    };
}

PanelEdgeBanding from_json_panel_edge_banding(const nlohmann::json& j) {
    auto side = [&](const char* key) -> std::optional<EdgeBanding> {
        const auto& v = j.at(key);
        if (v.is_null()) return std::nullopt;
        return from_json_edge_banding(v);
    };
    return PanelEdgeBanding{
        .front = side("front"),
        .back = side("back"),
        .left = side("left"),
        .right = side("right"),
    };
}

nlohmann::json to_json_hardware_spec(const HardwareSpec& s) {
    nlohmann::json j;
    j["ref"] = s.ref.value();
    j["kind"] = hardware_kind_to_str(s.kind);
    j["name"] = s.name;
    j["sku"] = s.sku ? nlohmann::json(*s.sku) : nlohmann::json(nullptr);
    j["bbox_mm"] = to_json_vec3(s.bbox);
    j["price_each"] = s.price_each ? to_json_money(*s.price_each) : nlohmann::json(nullptr);
    return j;
}

HardwareSpec from_json_hardware_spec(const nlohmann::json& j) {
    HardwareSpec s;
    s.ref = HardwareRef{j.at("ref").get<std::string>()};
    s.kind = hardware_kind_from_str(j.at("kind").get<std::string>());
    s.name = j.at("name").get<std::string>();
    if (!j.at("sku").is_null()) s.sku = j.at("sku").get<std::string>();
    s.bbox = from_json_vec3(j.at("bbox_mm"));
    if (!j.at("price_each").is_null()) s.price_each = from_json_money(j.at("price_each"));
    return s;
}

nlohmann::json to_json_panel_attachment(const PanelAttachment& a) {
    return nlohmann::json{
        {"panel_id", to_json_id(a.panel_id)},
        {"local_position_mm", to_json_vec3(a.local_position)},
        {"orientation", to_json_quat(a.orientation)},
    };
}

PanelAttachment from_json_panel_attachment(const nlohmann::json& j) {
    return PanelAttachment{
        .panel_id = from_json_id<PanelIdTag>(j.at("panel_id")),
        .local_position = from_json_vec3(j.at("local_position_mm")),
        .orientation = from_json_quat(j.at("orientation")),
    };
}

nlohmann::json to_json_hardware_item(const HardwareItem& h) {
    nlohmann::json atts = nlohmann::json::array();
    for (const auto& a : h.attachments) atts.push_back(to_json_panel_attachment(a));
    return nlohmann::json{
        {"id", to_json_id(h.id)},
        {"ref", h.ref.value()},
        {"attachments", atts},
        {"label", h.label ? nlohmann::json(*h.label) : nlohmann::json(nullptr)},
    };
}

HardwareItem from_json_hardware_item(const nlohmann::json& j) {
    HardwareItem h;
    h.id = from_json_id<HardwareItemIdTag>(j.at("id"));
    h.ref = HardwareRef{j.at("ref").get<std::string>()};
    for (const auto& a : j.at("attachments")) {
        h.attachments.push_back(from_json_panel_attachment(a));
    }
    if (!j.at("label").is_null()) h.label = j.at("label").get<std::string>();
    return h;
}

const char* panel_role_to_str(PanelRole r) { return panel_role_name(r); }

PanelRole panel_role_from_str(const std::string& s) {
    static const std::pair<const char*, PanelRole> table[] = {
        {"Top", PanelRole::Top}, {"Bottom", PanelRole::Bottom},
        {"SideLeft", PanelRole::SideLeft}, {"SideRight", PanelRole::SideRight},
        {"Back", PanelRole::Back}, {"Shelf", PanelRole::Shelf},
        {"DividerVertical", PanelRole::DividerVertical},
        {"DividerHorizontal", PanelRole::DividerHorizontal},
        {"Facade", PanelRole::Facade}, {"DrawerBottom", PanelRole::DrawerBottom},
        {"DrawerFront", PanelRole::DrawerFront},
        {"DrawerSide", PanelRole::DrawerSide},
        {"DrawerBack", PanelRole::DrawerBack},
        {"Plinth", PanelRole::Plinth}, {"Custom", PanelRole::Custom},
    };
    for (const auto& [n, r] : table) if (s == n) return r;
    throw InvalidData{"json.panel_role_unknown", "Unknown PanelRole: " + s};
}

const char* grain_direction_to_str(GrainDirection g) { return grain_direction_name(g); }

GrainDirection grain_direction_from_str(const std::string& s) {
    if (s == "Horizontal") return GrainDirection::Horizontal;
    if (s == "Vertical")   return GrainDirection::Vertical;
    if (s == "None")       return GrainDirection::None;
    throw InvalidData{"json.grain_unknown", "Unknown GrainDirection: " + s};
}

const char* hinge_side_to_str(HingeSide h) { return hinge_side_name(h); }

HingeSide hinge_side_from_str(const std::string& s) {
    if (s == "Left")   return HingeSide::Left;
    if (s == "Right")  return HingeSide::Right;
    if (s == "Top")    return HingeSide::Top;
    if (s == "Bottom") return HingeSide::Bottom;
    if (s == "None")   return HingeSide::None;
    throw InvalidData{"json.hinge_side_unknown", "Unknown HingeSide: " + s};
}

// RoleParams сериализуется как объект с kind-discriminator.
nlohmann::json to_json_role_params(const RoleParams& rp) {
    return std::visit([](const auto& p) -> nlohmann::json {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, NoRoleParams>) {
            return nlohmann::json{{"kind", "None"}};
        } else if constexpr (std::is_same_v<T, ShelfParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<ShelfFullWidth>(p.extent)) {
                ext = nlohmann::json{{"kind", "FullWidth"}};
            } else {
                const auto& bd = std::get<ShelfBetweenDividers>(p.extent);
                ext = nlohmann::json{{"kind", "BetweenDividers"},
                                      {"from", to_json_id(bd.from)},
                                      {"to", to_json_id(bd.to)}};
            }
            return nlohmann::json{
                {"kind", "Shelf"},
                {"height_from_bottom_mm", p.height_from_bottom.value()},
                {"extent", ext},
            };
        } else if constexpr (std::is_same_v<T, DividerVerticalParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<VerticalExtentFull>(p.height_extent)) {
                ext = nlohmann::json{{"kind", "Full"}};
            } else {
                const auto& r = std::get<VerticalExtentRange>(p.height_extent);
                ext = nlohmann::json{{"kind", "Range"},
                                      {"from_z_mm", r.from_z.value()},
                                      {"to_z_mm", r.to_z.value()}};
            }
            return nlohmann::json{
                {"kind", "DividerVertical"},
                {"offset_from_left_mm", p.offset_from_left.value()},
                {"height_extent", ext},
            };
        } else if constexpr (std::is_same_v<T, DividerHorizontalParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<DepthExtentFull>(p.depth_extent)) {
                ext = nlohmann::json{{"kind", "Full"}};
            } else {
                const auto& r = std::get<DepthExtentRange>(p.depth_extent);
                ext = nlohmann::json{{"kind", "Range"},
                                      {"from_y_mm", r.from_y.value()},
                                      {"to_y_mm", r.to_y.value()}};
            }
            return nlohmann::json{
                {"kind", "DividerHorizontal"},
                {"offset_from_bottom_mm", p.offset_from_bottom.value()},
                {"depth_extent", ext},
            };
        } else if constexpr (std::is_same_v<T, FacadeParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<FacadeFullFront>(p.extent)) {
                ext = nlohmann::json{{"kind", "FullFront"}};
            } else {
                const auto& r = std::get<FacadeRect>(p.extent);
                ext = nlohmann::json{{"kind", "Rect"},
                                      {"from_x_mm", r.from_x.value()},
                                      {"to_x_mm", r.to_x.value()},
                                      {"from_z_mm", r.from_z.value()},
                                      {"to_z_mm", r.to_z.value()}};
            }
            return nlohmann::json{
                {"kind", "Facade"},
                {"extent", ext},
                {"hinge_side", hinge_side_to_str(p.hinge_side)},
            };
        } else if constexpr (std::is_same_v<T, DrawerBottomParams>) {
            return nlohmann::json{{"kind", "DrawerBottom"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"depth_mm", p.depth.value()}};
        } else if constexpr (std::is_same_v<T, DrawerFrontParams>) {
            return nlohmann::json{{"kind", "DrawerFront"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()}};
        } else if constexpr (std::is_same_v<T, DrawerSideParams>) {
            return nlohmann::json{{"kind", "DrawerSide"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()},
                                   {"depth_mm", p.depth.value()},
                                   {"side", p.side == DrawerSideParams::Side::Left ? "Left" : "Right"}};
        } else if constexpr (std::is_same_v<T, DrawerBackParams>) {
            return nlohmann::json{{"kind", "DrawerBack"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()}};
        } else if constexpr (std::is_same_v<T, PlinthParams>) {
            return nlohmann::json{{"kind", "Plinth"},
                                   {"height_mm", p.height.value()},
                                   {"setback_mm", p.setback.value()}};
        } else if constexpr (std::is_same_v<T, CustomParams>) {
            return nlohmann::json{{"kind", "Custom"},
                                   {"position_mm", to_json_vec3(p.position)},
                                   {"size_mm", to_json_vec3(p.size)},
                                   {"orientation", to_json_quat(p.orientation)}};
        }
    }, rp);
}

RoleParams from_json_role_params(const nlohmann::json& j) {
    std::string kind = j.at("kind").get<std::string>();
    if (kind == "None")              return NoRoleParams{};
    if (kind == "Shelf") {
        ShelfParams sp;
        sp.height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm"));
        const auto& ext = j.at("extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "FullWidth")       sp.extent = ShelfFullWidth{};
        else if (ekind == "BetweenDividers") {
            sp.extent = ShelfBetweenDividers{
                .from = from_json_id<PanelIdTag>(ext.at("from")),
                .to = from_json_id<PanelIdTag>(ext.at("to")),
            };
        } else throw InvalidData{"json.shelf_extent_unknown", ekind};
        return sp;
    }
    if (kind == "DividerVertical") {
        DividerVerticalParams dp;
        dp.offset_from_left = from_json_millimeters(j.at("offset_from_left_mm"));
        const auto& ext = j.at("height_extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "Full") dp.height_extent = VerticalExtentFull{};
        else if (ekind == "Range") {
            dp.height_extent = VerticalExtentRange{
                .from_z = from_json_millimeters(ext.at("from_z_mm")),
                .to_z = from_json_millimeters(ext.at("to_z_mm"))};
        } else throw InvalidData{"json.vext_unknown", ekind};
        return dp;
    }
    if (kind == "DividerHorizontal") {
        DividerHorizontalParams dp;
        dp.offset_from_bottom = from_json_millimeters(j.at("offset_from_bottom_mm"));
        const auto& ext = j.at("depth_extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "Full") dp.depth_extent = DepthExtentFull{};
        else if (ekind == "Range") {
            dp.depth_extent = DepthExtentRange{
                .from_y = from_json_millimeters(ext.at("from_y_mm")),
                .to_y = from_json_millimeters(ext.at("to_y_mm"))};
        } else throw InvalidData{"json.dext_unknown", ekind};
        return dp;
    }
    if (kind == "Facade") {
        FacadeParams fp;
        const auto& ext = j.at("extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "FullFront") fp.extent = FacadeFullFront{};
        else if (ekind == "Rect") {
            fp.extent = FacadeRect{
                .from_x = from_json_millimeters(ext.at("from_x_mm")),
                .to_x = from_json_millimeters(ext.at("to_x_mm")),
                .from_z = from_json_millimeters(ext.at("from_z_mm")),
                .to_z = from_json_millimeters(ext.at("to_z_mm"))};
        } else throw InvalidData{"json.facade_extent_unknown", ekind};
        fp.hinge_side = hinge_side_from_str(j.at("hinge_side").get<std::string>());
        return fp;
    }
    if (kind == "DrawerBottom") {
        return DrawerBottomParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .depth = from_json_millimeters(j.at("depth_mm"))};
    }
    if (kind == "DrawerFront") {
        return DrawerFrontParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .height = from_json_millimeters(j.at("height_mm"))};
    }
    if (kind == "DrawerSide") {
        DrawerSideParams p;
        p.height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm"));
        p.height = from_json_millimeters(j.at("height_mm"));
        p.depth = from_json_millimeters(j.at("depth_mm"));
        std::string side = j.at("side").get<std::string>();
        if (side == "Left") p.side = DrawerSideParams::Side::Left;
        else if (side == "Right") p.side = DrawerSideParams::Side::Right;
        else throw InvalidData{"json.drawer_side_unknown", side};
        return p;
    }
    if (kind == "DrawerBack") {
        return DrawerBackParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .height = from_json_millimeters(j.at("height_mm"))};
    }
    if (kind == "Plinth") {
        return PlinthParams{
            .height = from_json_millimeters(j.at("height_mm")),
            .setback = from_json_millimeters(j.at("setback_mm"))};
    }
    if (kind == "Custom") {
        return CustomParams{
            .position = from_json_vec3(j.at("position_mm")),
            .size = from_json_vec3(j.at("size_mm")),
            .orientation = from_json_quat(j.at("orientation"))};
    }
    throw InvalidData{"json.role_params_unknown_kind", "Unknown RoleParams kind: " + kind};
}

nlohmann::json to_json_panel(const Panel& p) {
    nlohmann::json j;
    j["id"] = to_json_id(p.id);
    j["role"] = panel_role_to_str(p.role);
    j["role_params"] = to_json_role_params(p.role_params);
    j["material_override"] = p.material_override ? to_json_id(*p.material_override) : nlohmann::json(nullptr);
    j["thickness_override_mm"] = p.thickness_override ? nlohmann::json(p.thickness_override->value())
                                                      : nlohmann::json(nullptr);
    j["edge_banding"] = to_json_panel_edge_banding(p.edge_banding);
    j["grain_direction"] = grain_direction_to_str(p.grain_direction);
    j["label"] = p.label ? nlohmann::json(*p.label) : nlohmann::json(nullptr);
    return j;
}

Panel from_json_panel(const nlohmann::json& j) {
    Panel p;
    p.id = from_json_id<PanelIdTag>(j.at("id"));
    p.role = panel_role_from_str(j.at("role").get<std::string>());
    p.role_params = from_json_role_params(j.at("role_params"));
    if (!j.at("material_override").is_null()) {
        p.material_override = from_json_id<MaterialIdTag>(j.at("material_override"));
    }
    if (!j.at("thickness_override_mm").is_null()) {
        p.thickness_override = from_json_millimeters(j.at("thickness_override_mm"));
    }
    p.edge_banding = from_json_panel_edge_banding(j.at("edge_banding"));
    p.grain_direction = grain_direction_from_str(j.at("grain_direction").get<std::string>());
    if (!j.at("label").is_null()) p.label = j.at("label").get<std::string>();
    return p;
}

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
