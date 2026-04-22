#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>
#include <variant>

namespace coupecad::core {

// Все роли панелей (см. spec §2.2).
enum class PanelRole {
    Top,
    Bottom,
    SideLeft,
    SideRight,
    Back,
    Shelf,
    DividerVertical,
    DividerHorizontal,
    Facade,
    DrawerBottom,
    DrawerFront,
    DrawerSide,
    DrawerBack,
    Plinth,
    Custom,
};

const char* panel_role_name(PanelRole role) noexcept;

// --- RoleParams: variant per role ---

struct NoRoleParams {
    bool operator==(const NoRoleParams&) const = default;
};

// Полка может занимать всю ширину или быть зажата между
// двумя вертикальными перегородками (DividerVertical).
struct ShelfFullWidth {
    bool operator==(const ShelfFullWidth&) const = default;
};
struct ShelfBetweenDividers {
    PanelId from;
    PanelId to;
    bool operator==(const ShelfBetweenDividers&) const = default;
};
using ShelfExtent = std::variant<ShelfFullWidth, ShelfBetweenDividers>;

struct ShelfParams {
    Millimeters height_from_bottom{};
    ShelfExtent extent = ShelfFullWidth{};
    bool operator==(const ShelfParams&) const = default;
};

// Вертикальная перегородка: смещение от левой стенки + диапазон по высоте.
struct VerticalExtentFull {
    bool operator==(const VerticalExtentFull&) const = default;
};
struct VerticalExtentRange {
    Millimeters from_z{};
    Millimeters to_z{};
    bool operator==(const VerticalExtentRange&) const = default;
};
using VerticalExtent = std::variant<VerticalExtentFull, VerticalExtentRange>;

struct DividerVerticalParams {
    Millimeters offset_from_left{};
    VerticalExtent height_extent = VerticalExtentFull{};
    bool operator==(const DividerVerticalParams&) const = default;
};

// Горизонтальная перегородка: смещение от низа + диапазон по глубине.
struct DepthExtentFull {
    bool operator==(const DepthExtentFull&) const = default;
};
struct DepthExtentRange {
    Millimeters from_y{};
    Millimeters to_y{};
    bool operator==(const DepthExtentRange&) const = default;
};
using DepthExtent = std::variant<DepthExtentFull, DepthExtentRange>;

struct DividerHorizontalParams {
    Millimeters offset_from_bottom{};
    DepthExtent depth_extent = DepthExtentFull{};
    bool operator==(const DividerHorizontalParams&) const = default;
};

// Фасад: какую часть передней грани закрывает + сторона петель.
struct FacadeFullFront {
    bool operator==(const FacadeFullFront&) const = default;
};
struct FacadeRect {
    Millimeters from_x{};
    Millimeters to_x{};
    Millimeters from_z{};
    Millimeters to_z{};
    bool operator==(const FacadeRect&) const = default;
};
using FacadeExtent = std::variant<FacadeFullFront, FacadeRect>;

enum class HingeSide { Left, Right, Top, Bottom, None };
const char* hinge_side_name(HingeSide side) noexcept;

struct FacadeParams {
    FacadeExtent extent = FacadeFullFront{};
    HingeSide hinge_side = HingeSide::None;
    bool operator==(const FacadeParams&) const = default;
};

struct DrawerBottomParams {
    Millimeters height_from_bottom{};
    Millimeters depth{};
    bool operator==(const DrawerBottomParams&) const = default;
};
struct DrawerFrontParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    bool operator==(const DrawerFrontParams&) const = default;
};
struct DrawerSideParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    Millimeters depth{};
    enum class Side { Left, Right };
    Side side = Side::Left;
    bool operator==(const DrawerSideParams&) const = default;
};
struct DrawerBackParams {
    Millimeters height_from_bottom{};
    Millimeters height{};
    bool operator==(const DrawerBackParams&) const = default;
};

struct PlinthParams {
    Millimeters height{};
    Millimeters setback{};   // отступ от переднего края
    bool operator==(const PlinthParams&) const = default;
};

struct CustomParams {
    Vec3 position{};
    Vec3 size{};
    Quat orientation = Quat::identity();
    bool operator==(const CustomParams&) const = default;
};

using RoleParams = std::variant<
    NoRoleParams,
    ShelfParams,
    DividerVerticalParams,
    DividerHorizontalParams,
    FacadeParams,
    DrawerBottomParams,
    DrawerFrontParams,
    DrawerSideParams,
    DrawerBackParams,
    PlinthParams,
    CustomParams
>;

// --- EdgeBanding ---

struct EdgeBanding {
    MaterialId material_id;
    Millimeters thickness{2};

    bool operator==(const EdgeBanding&) const = default;
};

struct PanelEdgeBanding {
    std::optional<EdgeBanding> front;
    std::optional<EdgeBanding> back;
    std::optional<EdgeBanding> left;
    std::optional<EdgeBanding> right;

    bool operator==(const PanelEdgeBanding&) const = default;
};

enum class GrainDirection { Horizontal, Vertical, None };
const char* grain_direction_name(GrainDirection g) noexcept;

// --- Panel ---

struct Panel {
    PanelId id;
    PanelRole role = PanelRole::Top;
    RoleParams role_params = NoRoleParams{};
    std::optional<MaterialId> material_override;
    std::optional<Millimeters> thickness_override;
    PanelEdgeBanding edge_banding{};
    GrainDirection grain_direction = GrainDirection::None;
    std::optional<std::string> label;

    bool operator==(const Panel&) const = default;

    // Бросает DomainError если:
    //  - id не valid
    //  - role <-> role_params несовместимы (см. spec §3.4)
    //  - thickness_override <= 0
    //  - edge_banding имеет thickness <= 0 или material_id не valid
    void validate() const;
};

// Возвращает true если данная пара (role, role_params) допустима.
bool is_role_params_valid_for(PanelRole role, const RoleParams& params) noexcept;

}  // namespace coupecad::core
