#include "coupecad/ui/panel_properties_proxy.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

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
    // Full visitor lands in Task 11; placeholder for now.
    return QString{};
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

// Setters stubbed; filled in Task 10.
void PanelPropertiesProxy::setLabel(const QString&) {
    throw core::LogicError{"ui.not_implemented_yet", "setLabel in Task 10"};
}
void PanelPropertiesProxy::set_thickness_override_mm(int) {
    throw core::LogicError{"ui.not_implemented_yet", "set_thickness_override_mm in Task 10"};
}
void PanelPropertiesProxy::set_material_override_uuid(const QString&) {
    throw core::LogicError{"ui.not_implemented_yet", "set_material_override_uuid in Task 10"};
}
void PanelPropertiesProxy::clear_thickness_override() {
    throw core::LogicError{"ui.not_implemented_yet", "clear_thickness_override in Task 10"};
}
void PanelPropertiesProxy::clear_material_override() {
    throw core::LogicError{"ui.not_implemented_yet", "clear_material_override in Task 10"};
}

}  // namespace coupecad::ui
