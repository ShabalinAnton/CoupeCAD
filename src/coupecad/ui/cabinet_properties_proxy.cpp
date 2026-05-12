#include "coupecad/ui/cabinet_properties_proxy.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

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

void CabinetPropertiesProxy::setName(const QString& /*value*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setName fills in at Task 8"};
}
void CabinetPropertiesProxy::setWidthMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setWidthMm fills in at Task 7"};
}
void CabinetPropertiesProxy::setDepthMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setDepthMm fills in at Task 7"};
}
void CabinetPropertiesProxy::setHeightMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setHeightMm fills in at Task 7"};
}
void CabinetPropertiesProxy::set_default_panel_thickness_mm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "set_default_panel_thickness_mm fills in at Task 7"};
}
void CabinetPropertiesProxy::set_default_back_thickness_mm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "set_default_back_thickness_mm fills in at Task 7"};
}

}  // namespace coupecad::ui
