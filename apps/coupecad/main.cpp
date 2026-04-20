#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include <iostream>

namespace {
constexpr const char* kAppVersion = "0.1.0";
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CoupeCAD"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));
    app.setOrganizationName(QStringLiteral("CoupeCAD"));
    app.setOrganizationDomain(QStringLiteral("coupecad.app"));

    // Простой ручной разбор `--version`/`-v` без QCommandLineParser ради
    // минимума зависимостей в Stage 0.
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--version") || arg == QStringLiteral("-v")) {
            std::cout << "CoupeCAD " << kAppVersion << std::endl;
            return 0;
        }
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    engine.loadFromModule("coupecad", "Main");
    return app.exec();
}
