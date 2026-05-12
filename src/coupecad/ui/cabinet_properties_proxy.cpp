#include "coupecad/ui/cabinet_properties_proxy.h"

#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

#include <memory>

namespace coupecad::ui {

CabinetPropertiesProxy::CabinetPropertiesProxy(
    core::Project& project, core::UndoStack& undo,
    viewport::ViewportController& controller, QObject* parent)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::cabinetChanged,
                     this,         &CabinetPropertiesProxy::on_cabinet_changed);
    coupecad::logging::Logger::instance().info(
        "ui", "CabinetPropertiesProxy constructed");
}

void CabinetPropertiesProxy::on_cabinet_changed() {
    emit changed();
}

QString CabinetPropertiesProxy::name() const {
    return QString::fromStdString(project_.cabinet().name);
}

int CabinetPropertiesProxy::widthMm() const {
    return project_.cabinet().dimensions.width.value();
}

int CabinetPropertiesProxy::depthMm() const {
    return project_.cabinet().dimensions.depth.value();
}

int CabinetPropertiesProxy::heightMm() const {
    return project_.cabinet().dimensions.height.value();
}

int CabinetPropertiesProxy::default_panel_thickness_mm() const {
    return project_.cabinet().default_panel_thickness.value();
}

int CabinetPropertiesProxy::default_back_thickness_mm() const {
    return project_.cabinet().default_back_thickness.value();
}

void CabinetPropertiesProxy::setName(const QString& value) {
    const auto& cab = project_.cabinet();
    const std::string new_name = value.toStdString();
    if (new_name == cab.name) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetName>(cab.id, new_name));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.name rejected: {}", e.what());
        emit changed();
    }
}
void CabinetPropertiesProxy::setWidthMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.width.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.width = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.width rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::setDepthMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.depth.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.depth = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.depth rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::setHeightMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.height.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.height = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.height rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::set_default_panel_thickness_mm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.default_panel_thickness.value()) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetDefaults>(
            cab.id,
            cab.default_panel_material,
            core::Millimeters{mm},
            cab.default_back_thickness));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.default_panel_thickness rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::set_default_back_thickness_mm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.default_back_thickness.value()) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetDefaults>(
            cab.id,
            cab.default_panel_material,
            cab.default_panel_thickness,
            core::Millimeters{mm}));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.default_back_thickness rejected: {}", e.what());
        emit changed();
    }
}

}  // namespace coupecad::ui
