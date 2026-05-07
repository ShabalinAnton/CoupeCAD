#include "coupecad/viewport/occt_viewport_item.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;
QGuiApplication* s_qapp = nullptr;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "viewport_qml_smoke";
    static char*  argv[] = {arg0, nullptr};
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    s_qapp = new QGuiApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

TEST(ViewportQmlSmokeTest, RegisterTypeAndInstantiate) {
    ensure_qapp();
    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        import QtQuick.Window
        import coupecad

        Window {
            visible: false
            OcctViewportItem {
                anchors.fill: parent
            }
        }
    )");
    ASSERT_FALSE(engine.rootObjects().isEmpty());
}
