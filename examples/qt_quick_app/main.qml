import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Window {
    id: root
    objectName: "rootWindow"
    visible: true
    width: 520
    height: 480
    title: "Qt Quick - Injection Test Stand"

    ColumnLayout {
        objectName: "mainColumn"
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            objectName: "infoLabel"
            text: "Agent output goes to stderr. Interact with elements below."
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        // ── Tab bar ──────────────────────────────────────────────────────
        TabBar {
            id: tabBar
            objectName: "tabBar"
            Layout.fillWidth: true

            TabButton {
                objectName: "tabInput"
                text: "Input"
            }
            TabButton {
                objectName: "tabControls"
                text: "Controls"
            }
            TabButton {
                objectName: "tabButtons"
                text: "Buttons"
            }
        }

        StackLayout {
            objectName: "stackLayout"
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ── Page 1: Input ────────────────────────────────────────────
            ColumnLayout {
                objectName: "inputPage"
                spacing: 8

                TextField {
                    id: nameTextField

                    objectName: "nameField"
                    placeholderText: "Enter your name..."
                    Layout.fillWidth: true
                }

                TextField {
                    id: messageTextField

                    objectName: "messageField"
                    placeholderText: "Enter a message..."
                    Layout.fillWidth: true
                }

                Label {
                    id: greetLabel
                    objectName: "greetLabel"
                    text: ""
                    Layout.fillWidth: true
                }

                Button {
                    objectName: "btnGreet"
                    text: "Greet"
                    Layout.fillWidth: true
                    onClicked: {
                        greetLabel.text = nameTextField.text.trim() + " says: " + messageTextField.text.trim();
                    }
                }
            }

            // ── Page 2: Controls ─────────────────────────────────────────
            ColumnLayout {
                objectName: "controlsPage"
                spacing: 8

                CheckBox {
                    objectName: "featureCheck"
                    text: "Enable feature"
                }

                // Intentionally unnamed checkbox - tests fallback path
                CheckBox {
                    text: "Another option"
                }

                Label {
                    objectName: "radioLabel"
                    text: "Select option:"
                }

                RadioButton {
                    objectName: "radioA"
                    text: "Option A"
                    checked: true
                }
                RadioButton {
                    objectName: "radioB"
                    text: "Option B"
                }
                // Intentionally unnamed
                RadioButton {
                    text: "Option C"
                }

                ComboBox {
                    objectName: "comboBox"
                    model: ["First", "Second", "Third"]
                    Layout.fillWidth: true
                }
            }

            // ── Page 3: Buttons ──────────────────────────────────────────
            ColumnLayout {
                id: buttonsPageLayout

                objectName: "buttonsPage"
                spacing: 8

                Label {
                    id: clickCount
                    objectName: "clickCount"
                    text: "Clicks: 0"
                }

                property int counter: 0

                RowLayout {
                    objectName: "buttonRow"
                    spacing: 8

                    Button {
                        objectName: "btn1"
                        text: "Button 1"
                        onClicked: {
                            buttonsPageLayout.counter++;
                            clickCount.text = "Clicks: " + buttonsPageLayout.counter;
                        }
                    }
                    // Intentionally unnamed
                    Button {
                        text: "Button 2"
                        onClicked: {
                            buttonsPageLayout.counter++;
                            clickCount.text = "Clicks: " + buttonsPageLayout.counter;
                        }
                    }
                    Button {
                        objectName: "btn3"
                        text: "Button 3"
                        onClicked: {
                            buttonsPageLayout.counter++;
                            clickCount.text = "Clicks: " + buttonsPageLayout.counter;
                        }
                    }
                }

                Button {
                    objectName: "btnReset"
                    text: "Reset"
                    Layout.fillWidth: true
                    onClicked: {
                        buttonsPageLayout.counter = 0;
                        clickCount.text = "Clicks: 0";
                    }
                }
            }
        }
    }
}
