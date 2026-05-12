#pragma once

#include "coupecad/core/project.h"
#include "coupecad/viewport/viewport_controller.h"

#include <QObject>
#include <QString>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

class CabinetPropertiesProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name      READ name      WRITE setName      NOTIFY changed)
    Q_PROPERTY(int     widthMm   READ widthMm   WRITE setWidthMm   NOTIFY changed)
    Q_PROPERTY(int     depthMm   READ depthMm   WRITE setDepthMm   NOTIFY changed)
    Q_PROPERTY(int     heightMm  READ heightMm  WRITE setHeightMm  NOTIFY changed)
    Q_PROPERTY(int     defaultPanelThicknessMm
                 READ default_panel_thickness_mm
                 WRITE set_default_panel_thickness_mm NOTIFY changed)
    Q_PROPERTY(int     defaultBackThicknessMm
                 READ default_back_thickness_mm
                 WRITE set_default_back_thickness_mm NOTIFY changed)

public:
    CabinetPropertiesProxy(core::Project& project,
                           core::UndoStack& undo,
                           viewport::ViewportController& controller,
                           QObject* parent = nullptr);

    QString name() const;
    int     widthMm() const;
    int     depthMm() const;
    int     heightMm() const;
    int     default_panel_thickness_mm() const;
    int     default_back_thickness_mm() const;

    void setName(const QString& value);
    void setWidthMm(int mm);
    void setDepthMm(int mm);
    void setHeightMm(int mm);
    void set_default_panel_thickness_mm(int mm);
    void set_default_back_thickness_mm(int mm);

signals:
    void changed();

private slots:
    void on_cabinet_changed();

private:
    core::Project&                project_;
    core::UndoStack&              undo_;
    viewport::ViewportController& controller_;
};

}  // namespace coupecad::ui
