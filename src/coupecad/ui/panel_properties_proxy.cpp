#include "coupecad/ui/panel_properties_proxy.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

#include <memory>
#include <type_traits>
#include <variant>

namespace coupecad::ui {

PanelPropertiesProxy::PanelPropertiesProxy(
    core::Project& project, core::UndoStack& undo,
    viewport::ViewportController& controller, QObject* parent)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::selectionChanged,
                     this,         &PanelPropertiesProxy::on_selection_changed);
    QObject::connect(&controller_, &viewport::ViewportController::panelChanged,
                     this,         &PanelPropertiesProxy::on_panel_changed);
    coupecad::logging::Logger::instance().info(
        "ui", "PanelPropertiesProxy constructed");
}

void PanelPropertiesProxy::on_selection_changed() {
    emit changed();
}

void PanelPropertiesProxy::on_panel_changed(const QString& panel_id) {
    const auto* p = current_panel();
    if (p != nullptr && QString::fromStdString(p->id.to_string()) == panel_id) {
        emit changed();
    }
}

const core::Panel* PanelPropertiesProxy::current_panel() const {
    const auto sel = controller_.selection();
    if (sel.size() != 1) return nullptr;
    const auto* pid_ptr = std::get_if<core::PanelId>(&sel[0]);
    if (pid_ptr == nullptr) return nullptr;
    const auto it = project_.cabinet().panels.find(*pid_ptr);
    return it == project_.cabinet().panels.end() ? nullptr : &it->second;
}

bool PanelPropertiesProxy::has_panel() const {
    return current_panel() != nullptr;
}

QString PanelPropertiesProxy::panel_id_string() const {
    const auto* p = current_panel();
    return p == nullptr ? QString{} : QString::fromStdString(p->id.to_string());
}

QString PanelPropertiesProxy::role() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};
    return QString::fromUtf8(core::panel_role_name(p->role));
}

QString PanelPropertiesProxy::role_params_summary() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};

    return std::visit([](const auto& v) -> QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, core::NoRoleParams>) {
            return QString{};
        } else if constexpr (std::is_same_v<T, core::ShelfParams>) {
            QString extent_str =
                std::holds_alternative<core::ShelfFullWidth>(v.extent)
                    ? QStringLiteral("full-width")
                    : QStringLiteral("between dividers");
            return QString("h=%1 mm, %2")
                .arg(v.height_from_bottom.value())
                .arg(extent_str);
        } else if constexpr (std::is_same_v<T, core::DividerVerticalParams>) {
            return QString("offset=%1 mm, %2")
                .arg(v.offset_from_left.value())
                .arg(std::holds_alternative<core::VerticalExtentFull>(v.height_extent)
                         ? QStringLiteral("full height")
                         : QStringLiteral("range"));
        } else if constexpr (std::is_same_v<T, core::DividerHorizontalParams>) {
            return QString("offset=%1 mm, %2")
                .arg(v.offset_from_bottom.value())
                .arg(std::holds_alternative<core::DepthExtentFull>(v.depth_extent)
                         ? QStringLiteral("full depth")
                         : QStringLiteral("range"));
        } else {
            return QStringLiteral("(see role-params)");
        }
    }, p->role_params);
}

QString PanelPropertiesProxy::label() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};
    return p->label.has_value()
        ? QString::fromStdString(*p->label)
        : QString{};
}

bool PanelPropertiesProxy::has_thickness_override() const {
    const auto* p = current_panel();
    return p != nullptr && p->thickness_override.has_value();
}

int PanelPropertiesProxy::thickness_override_mm() const {
    const auto* p = current_panel();
    if (p == nullptr) return 0;
    if (p->thickness_override.has_value()) {
        return p->thickness_override->value();
    }
    return project_.cabinet().default_panel_thickness.value();
}

bool PanelPropertiesProxy::has_material_override() const {
    const auto* p = current_panel();
    return p != nullptr && p->material_override.has_value();
}

QString PanelPropertiesProxy::material_override_uuid() const {
    const auto* p = current_panel();
    if (p == nullptr || !p->material_override.has_value()) return QString{};
    return QString::fromStdString(p->material_override->to_string());
}

void PanelPropertiesProxy::setLabel(const QString& value) {
    const auto* p = current_panel();
    if (p == nullptr) return;

    std::optional<std::string> new_label;
    if (!value.isEmpty()) new_label = value.toStdString();
    if (new_label == p->label) return;

    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelLabel>(pid, new_label));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.label rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::set_thickness_override_mm(int mm) {
    const auto* p = current_panel();
    if (p == nullptr) return;
    const auto pid = p->id;
    const std::optional<core::Millimeters> new_value = core::Millimeters{mm};
    if (new_value == p->thickness_override) return;

    try {
        undo_.execute(std::make_unique<core::SetPanelThickness>(pid, new_value));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.thickness rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::clear_thickness_override() {
    const auto* p = current_panel();
    if (p == nullptr) return;
    if (!p->thickness_override.has_value()) return;

    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelThickness>(pid, std::nullopt));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.thickness clear rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::set_material_override_uuid(const QString& uuid_or_empty) {
    const auto* p = current_panel();
    if (p == nullptr) return;
    const auto pid = p->id;

    std::optional<core::MaterialId> new_value;
    if (!uuid_or_empty.isEmpty()) {
        auto parsed = core::MaterialId::from_string(uuid_or_empty.toStdString());
        if (!parsed.is_valid()) {
            coupecad::logging::Logger::instance().warn(
                "ui", "panel.material_override invalid UUID '{}'",
                uuid_or_empty.toStdString());
            emit changed();
            return;
        }
        new_value = parsed;
    }
    if (new_value == p->material_override) return;

    try {
        undo_.execute(std::make_unique<core::SetPanelMaterial>(pid, new_value));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.material rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::clear_material_override() {
    const auto* p = current_panel();
    if (p == nullptr) return;
    if (!p->material_override.has_value()) return;
    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelMaterial>(pid, std::nullopt));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.material clear rejected: {}", e.what());
        emit changed();
    }
}

}  // namespace coupecad::ui
