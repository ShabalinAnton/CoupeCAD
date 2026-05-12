import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var proxy

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Identity (read-only)
        Label { text: qsTr("ID"); font.pixelSize: 10; color: "gray" }
        TextEdit {
            Layout.fillWidth: true
            readOnly: true
            selectByMouse: true
            text: proxy.panelIdString
            font.family: "monospace"
            font.pixelSize: 10
        }

        Label { text: qsTr("Role"); Layout.topMargin: 6 }
        Label { text: proxy.role; font.bold: true }

        Label { text: qsTr("Role params") }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: proxy.roleParamsSummary
            visible: proxy.roleParamsSummary.length > 0
        }

        // Label
        Label { text: qsTr("Label"); Layout.topMargin: 8 }
        TextField {
            Layout.fillWidth: true
            text: proxy.label
            onEditingFinished: proxy.label = text
        }

        // Thickness override
        Label { text: qsTr("Thickness (mm)"); Layout.topMargin: 8 }
        RowLayout {
            Layout.fillWidth: true
            SpinBox {
                id: thicknessSpin
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.thicknessOverrideMm
                onValueModified: proxy.thicknessOverrideMm = value
            }
            Button {
                text: qsTr("Use default")
                enabled: proxy.hasThicknessOverride
                onClicked: proxy.clear_thickness_override()
            }
        }
        Label {
            visible: !proxy.hasThicknessOverride
            text: qsTr("(inherited from cabinet)")
            font.italic: true
            color: "gray"
        }

        // Material override
        Label { text: qsTr("Material override (UUID)"); Layout.topMargin: 8 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                Layout.fillWidth: true
                text: proxy.materialOverrideUuid
                placeholderText: qsTr("(inherited)")
                onEditingFinished: proxy.materialOverrideUuid = text
            }
            Button {
                text: qsTr("Clear")
                enabled: proxy.hasMaterialOverride
                onClicked: proxy.clear_material_override()
            }
        }

        Item { Layout.fillHeight: true }
    }
}
