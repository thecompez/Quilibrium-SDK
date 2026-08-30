import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 720
    height: 900
    visible: true
    title: "Quilibrium SDK / HyperSnap"

    Component.onCompleted: feedController.loadTrending()

    ListView {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12
        model: feedController.casts
        delegate: Frame {
            required property var modelData
            width: ListView.view.width
            Column {
                width: parent.width
                spacing: 6
                Label { text: modelData.displayName + "  @" + modelData.username; font.bold: true }
                Label { width: parent.width; text: modelData.text; wrapMode: Text.Wrap }
                Label { text: "♥ " + modelData.likes }
            }
        }
    }
}
