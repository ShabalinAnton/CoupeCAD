#pragma once

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/viewport/viewport_controller.h"

#include <QObject>
#include <QString>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

class PanelPropertiesProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool    hasPanel      READ has_panel      NOTIFY changed)
    Q_PROPERTY(QString panelIdString READ panel_id_string NOTIFY changed)
    Q_PROPERTY(QString role          READ role           NOTIFY changed)
    Q_PROPERTY(QString roleParamsSummary
                 READ role_params_summary NOTIFY changed)

    Q_PROPERTY(QString label
                 READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(bool    hasThicknessOverride
                 READ has_thickness_override NOTIFY changed)
    Q_PROPERTY(int     thicknessOverrideMm
                 READ thickness_override_mm
                 WRITE set_thickness_override_mm NOTIFY changed)
    Q_PROPERTY(bool    hasMaterialOverride
                 READ has_material_override NOTIFY changed)
    Q_PROPERTY(QString materialOverrideUuid
                 READ material_override_uuid
                 WRITE set_material_override_uuid NOTIFY changed)

public:
    PanelPropertiesProxy(core::Project& project,
                         core::UndoStack& undo,
                         viewport::ViewportController& controller,
                         QObject* parent = nullptr);

    bool    has_panel() const;
    QString panel_id_string() const;
    QString role() const;
    QString role_params_summary() const;
    QString label() const;
    bool    has_thickness_override() const;
    int     thickness_override_mm() const;
    bool    has_material_override() const;
    QString material_override_uuid() const;

    void setLabel(const QString& value);
    void set_thickness_override_mm(int mm);
    void set_material_override_uuid(const QString& uuid_or_empty);

    Q_INVOKABLE void clear_thickness_override();
    Q_INVOKABLE void clear_material_override();

signals:
    void changed();

private slots:
    void on_selection_changed();
    void on_panel_changed(const QString& panel_id);

private:
    const core::Panel* current_panel() const;

    core::Project&                project_;
    core::UndoStack&              undo_;
    viewport::ViewportController& controller_;
};

}  // namespace coupecad::ui
