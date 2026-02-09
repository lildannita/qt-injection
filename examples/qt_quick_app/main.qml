import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Window {
    id: root

    property int spawnCounter: 0

    objectName: "rootWindow"
    visible: true
    width: 480
    height: 420
    title: "Qt Quick - Injection Test Stand"

    ColumnLayout {
        objectName: "mainColumn"
        anchors {
            fill: parent
            margins: 16
        }

        spacing: 12

        Label {
            Layout.fillWidth: true
            text: "Agent diagnostic output goes to stderr (console)."
            wrapMode: Text.Wrap
        }

        TextField {
            id: inputField

            objectName: "inputField"
            Layout.fillWidth: true
            placeholderText: "Type something..."
        }

        Button {
            objectName: "btnGreet"
            Layout.fillWidth: true
            text: "Greet"
            onClicked: {
                infoLabel2.text = "Hello from " + inputField.text;
            }
        }

        Label {
            id: infoLabel2

            Layout.fillWidth: true
        }

        // Dynamic creation area
        Rectangle {
            objectName: "dynamicArea"
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#F0F0F0"
            border.color: "#ccc"
            radius: 4

            ColumnLayout {
                anchors {
                    fill: parent
                    margins: 8
                }

                spacing: 6

                Label {
                    objectName: "dynamicAreaLabel"
                    text: "Dynamic items appear below (auto every 3 s, removed after 5 s):"
                    wrapMode: Text.Wrap
                }

                // Container for dynamically spawned items
                Column {
                    id: dynamicColumn

                    objectName: "dynamicColumn"
                    Layout.fillWidth: true
                    spacing: 4
                }
            }
        }

        Button {
            objectName: "btnSpawn"
            text: "Spawn item manually"
            Layout.fillWidth: true
            onClicked: root.spawnItem()
        }
    }

    // ── Dynamic item creation ────────────────────────────────────────────



    Component {
        id: dynamicRect

        Rectangle {
            width: dynamicColumn.width
            height: 30
            radius: 3
            color: Qt.rgba(Math.random(), Math.random(), 0.8, 0.4)

            Label {
                anchors.centerIn: parent
                text: parent.objectName
            }
        }
    }

    function spawnItem() {
        root.spawnCounter++;
        var obj = dynamicRect.createObject(dynamicColumn, {
            "objectName": "dynItem_" + spawnCounter
        });
        // Auto-destroy after 5 seconds
        obj.Component.destruction.connect(function() {});
        var timer = Qt.createQmlObject(
            'import QtQuick 2.15; Timer { interval: 5000; running: true; repeat: false }',
            obj, "destroyTimer");
        timer.objectName = "destroyTimer_" + root.spawnCounter;
        timer.triggered.connect(function() { obj.destroy() });
    }

    // Auto-spawn every 3 seconds
    Timer {
        objectName: "autoSpawnTimer"
        interval: 3000
        running: true
        repeat: true
        onTriggered: root.spawnItem()
    }
}
