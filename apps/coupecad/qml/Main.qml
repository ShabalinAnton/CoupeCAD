import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1024
    height: 720
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"
    }
}
