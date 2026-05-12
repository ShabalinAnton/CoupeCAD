import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import coupecad

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    Shortcut { sequence: StandardKey.Undo; onActivated: undoStack.undo() }
    Shortcut { sequence: StandardKey.Redo; onActivated: undoStack.redo() }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        OcctViewportItem {
            id: viewport
            SplitView.fillWidth: true
            SplitView.minimumWidth: 400
            focus: true
            Component.onCompleted: viewport.set_controller(viewportController)
        }

        Pane {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 240
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: panelProperties.hasPanel
                          ? qsTr("Selected panel")
                          : qsTr("Cabinet")
                    font.pixelSize: 16
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: panelProperties.hasPanel
                          ? qsTr("Panel inspector (Task 14)")
                          : qsTr("Cabinet inspector (Task 13)")
                    color: "gray"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
