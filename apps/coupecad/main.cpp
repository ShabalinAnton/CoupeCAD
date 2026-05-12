#include "demo_project.h"

#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/occt/occt_renderer.h"
#include "coupecad/ui/cabinet_properties_proxy.h"
#include "coupecad/ui/panel_properties_proxy.h"
#include "coupecad/ui/undo_stack_proxy.h"
#include "coupecad/viewport/occt_viewport_item.h"
#include "coupecad/viewport/viewport_controller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>

#include <iostream>
#include <memory>

namespace { constexpr const char* kAppVersion = "0.1.0"; }

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CoupeCAD"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setOrganizationName(QStringLiteral("CoupeCAD"));
    app.setOrganizationDomain(QStringLiteral("coupecad.app"));

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QStringLiteral("--version") || a == QStringLiteral("-v")) {
            std::cout << "CoupeCAD " << kAppVersion << std::endl;
            return 0;
        }
    }

    // Data layer.
    auto project = std::make_unique<coupecad::core::Project>(
        coupecad::app::make_demo_project());
    auto undo    = std::make_unique<coupecad::core::UndoStack>(*project);
    auto builder = std::make_unique<coupecad::geometry::GeometryBuilder>(
        *project);
    auto renderer = coupecad::renderer::occt::make_occt_renderer(
        *project, *builder);
    auto controller = std::make_unique<coupecad::viewport::ViewportController>(
        *project, *builder, *renderer);
    undo->add_observer(controller.get());

    auto cabinet_props = std::make_unique<coupecad::ui::CabinetPropertiesProxy>(
        *project, *undo, *controller);
    auto panel_props = std::make_unique<coupecad::ui::PanelPropertiesProxy>(
        *project, *undo, *controller);
    auto undo_proxy = std::make_unique<coupecad::ui::UndoStackProxy>(*undo);

    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        "viewportController", QVariant::fromValue(controller.get()));
    engine.rootContext()->setContextProperty(
        "cabinetProperties",  QVariant::fromValue(cabinet_props.get()));
    engine.rootContext()->setContextProperty(
        "panelProperties",    QVariant::fromValue(panel_props.get()));
    engine.rootContext()->setContextProperty(
        "undoStack",          QVariant::fromValue(undo_proxy.get()));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    engine.loadFromModule("coupecad", "Main");
    return app.exec();
}
