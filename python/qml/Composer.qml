import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: composerRoot
    width: parent.width - 40
    height: Math.max(80, inputRow.implicitHeight + 40)
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 20
    
    radius: 16
    color: geminiBridge.glassBackground
    border.color: "#334155"
    border.width: 1

    signal sendMessage(string text)
    signal stopRun()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Status Banner (integrated)
        Rectangle {
            Layout.fillWidth: true
            height: 24
            visible: geminiBridge.isWorking
            color: "transparent"

            RowLayout {
                anchors.centerIn: parent
                spacing: 8

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    PropertyAnimation { from: 0.4; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                    PropertyAnimation { from: 1.0; to: 0.4; duration: 800; easing.type: Easing.InOutQuad }
                }

                Text {
                    text: geminiBridge.thinkingText || "VIORA THINKING..."
                    color: "#3b82f6"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }
            }
        }

        RowLayout {
            id: inputRow
            Layout.fillWidth: true
            spacing: 12

            ScrollView {
                Layout.fillWidth: true
                Layout.maximumHeight: 150
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                TextArea {
                    id: inputField
                    placeholderText: "Message Viora AI... (Enter to send)"
                    placeholderTextColor: "#4b5563"
                    color: "white"
                    font.pixelSize: 14
                    wrapMode: TextArea.Wrap
                    background: null
                    
                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ControlModifier)) {
                            if (text.trim() !== "") {
                                composerRoot.sendMessage(text.trim())
                                text = ""
                            }
                            event.accepted = true
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // Left side controls
            Button {
                text: "REFRESH"
                onClicked: geminiBridge.refreshModels()
                flat: true
                font.pixelSize: 10
                font.bold: true
                contentItem: Text {
                    text: parent.text
                    color: "#94a3b8"
                    font: parent.font
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ComboBox {
                model: geminiBridge.availableModels
                currentIndex: model.indexOf(geminiBridge.currentModel)
                onActivated: (index) => geminiBridge.currentModel = model[index]
                Layout.preferredWidth: 120
                flat: true
                // Custom styling for the glassy combo
            }

            Item { Layout.fillWidth: true }

            // Action buttons
            Button {
                id: stopBtn
                text: "STOP"
                visible: geminiBridge.isWorking
                onClicked: composerRoot.stopRun()
                background: Rectangle {
                    radius: 8
                    color: "#ef4444"
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            Button {
                id: sendBtn
                text: "SEND"
                visible: !geminiBridge.isWorking
                enabled: inputField.text.trim() !== ""
                onClicked: {
                    composerRoot.sendMessage(inputField.text.trim())
                    inputField.text = ""
                }
                background: Rectangle {
                    radius: 8
                    color: parent.enabled ? "#2563eb" : "#1e293b"
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "white" : "#475569"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }
}
