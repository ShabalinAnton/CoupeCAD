import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var proxy

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Label { text: qsTr("Name") }
        TextField {
            Layout.fillWidth: true
            text: proxy.name
            onEditingFinished: proxy.name = text
        }

        Label { text: qsTr("Dimensions (mm)"); Layout.topMargin: 8 }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label  { text: qsTr("Width")  }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.widthMm
                onValueModified: proxy.widthMm = value
            }
            Label  { text: qsTr("Depth")  }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.depthMm
                onValueModified: proxy.depthMm = value
            }
            Label  { text: qsTr("Height") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.heightMm
                onValueModified: proxy.heightMm = value
            }
        }

        Label { text: qsTr("Defaults"); Layout.topMargin: 8 }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label  { text: qsTr("Panel thickness") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.defaultPanelThicknessMm
                onValueModified: proxy.defaultPanelThicknessMm = value
            }
            Label  { text: qsTr("Back thickness") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.defaultBackThicknessMm
                onValueModified: proxy.defaultBackThicknessMm = value
            }
        }

        Item { Layout.fillHeight: true }   // spacer
    }
}
