import QtQuick
import QtQuick.Window
import coupecad

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    OcctViewportItem {
        id: viewport
        anchors.fill: parent
        focus: true

        Component.onCompleted: viewport.set_controller(viewportController)
    }

    Text {
        anchors { left: parent.left; top: parent.top; margins: 8 }
        text: viewport.selectionEmpty
              ? qsTr("No selection")
              : qsTr("Selected: ") + viewport.selectionCount
        color: "white"
        font.pixelSize: 14
        style: Text.Outline
        styleColor: "black"
    }
}
