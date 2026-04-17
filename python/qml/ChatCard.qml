import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root
    width: parent ? parent.width : 300
    height: cardContainer.height + 4

    // Explicitly use modelData for reliable role mapping from QVariantMap
    readonly property bool isUser: (modelData && modelData.role === "user")
    readonly property bool isAction: (modelData && modelData.role === "action")
    readonly property var messageParts: (modelData && modelData.parts) ? modelData.parts : []
    readonly property string messageContent: (modelData && modelData.content) ? modelData.content : (modelData && modelData.text ? modelData.text : "")
    readonly property string messageThought: (modelData && modelData.thought) ? modelData.thought : ""
    readonly property string messageTimestamp: (modelData && modelData.timestamp) ? modelData.timestamp : ""

    property bool thoughtExpanded: false
    signal showDashboardRequested()

    MessageDialog {
        id: undoConfirmDialog
        title: "Rewind Conversation"
        text: "Are you sure you want to rewind the chat to this point? This will also undo any schematic changes made after this message."
        buttons: MessageDialog.Yes | MessageDialog.No
        onButtonClicked: (button, role) => {
            if (button === MessageDialog.Yes) {
                geminiBridge.undoToPoint(index)
            }
        }
    }
    
    onMessageThoughtChanged: {
        if (messageThought !== "" && !thoughtExpanded) {
            thoughtExpanded = true
        }
    }

    Rectangle {
        id: cardContainer
        width: Math.min(parent.width * 0.9, 600)
        height: contentColumn.implicitHeight + (isUser ? 12 : 8)
        anchors.left: parent.left
        anchors.leftMargin: 10
        
        radius: 6
        clip: false // Allow hover icon to slightly overlap
        
        HoverHandler {
            id: hoverHandler
        }

        // Undo Icon (Visible on hover)
        Rectangle {
            id: undoBtn
            visible: isUser && hoverHandler.hovered
            width: 24; height: 24; radius: 4
            color: Qt.rgba(30, 41, 59, 0.9)
            border.color: "#3b82f6"
            border.width: 1
            
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 4
            z: 10
            
            Text {
                anchors.centerIn: parent
                text: "↺"
                color: "white"
                font.bold: true
                font.pixelSize: 14
            }
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: undoConfirmDialog.open()
                ToolTip.visible: parent.visible && containsMouse
                ToolTip.text: "Undo/Rewind to here"
                ToolTip.delay: 400
            }
        }
        color: {
            if (isUser) {
                return (typeof geminiBridge !== "undefined" && geminiBridge && geminiBridge.glassBackground) ? geminiBridge.glassBackground : "rgba(15, 23, 42, 0.95)";
            }
            return "transparent"; // AI response is direct text
        }
        border.color: isUser ? "#334155" : "transparent"
        border.width: isUser ? 1 : 0

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: isUser ? 8 : 4
            spacing: 4

            RowLayout {
                id: headerRow
                Layout.fillWidth: true
                spacing: 8
                visible: false // Hidden by default
            }

            // Thought Process (Collapse/Expand)
            ColumnLayout {
                Layout.fillWidth: true
                visible: !isUser && messageThought !== ""
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    
                    Text {
                        text: thoughtExpanded ? "▼ THOUGHTS" : "▶ THOUGHTS"
                        color: "#60a5fa"
                        font.pixelSize: 10 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                        font.bold: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: thoughtExpanded = !thoughtExpanded
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1,1,1,0.05) }
                }

                Text {
                    Layout.fillWidth: true
                    visible: thoughtExpanded
                    text: messageThought
                    color: "#94a3b8"
                    font.pixelSize: 12 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                    font.italic: true
                    wrapMode: Text.Wrap
                }
            }

            // Main Content Area (Structured Parts)
            ColumnLayout {
                id: partsContainer
                Layout.fillWidth: true
                spacing: 12

                // User-attached image
                Image {
                    visible: modelData.image !== undefined && modelData.image !== ""
                    source: modelData.image ? "data:image/png;base64," + modelData.image : ""
                    Layout.maximumWidth: parent.width * 0.8
                    Layout.preferredHeight: 200
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    cache: true
                }

                Repeater {
                    model: messageParts
                    delegate: Loader {
                        Layout.fillWidth: true
                        sourceComponent: {
                            if (!modelData) return textComponent;
                            switch(modelData.type) {
                                case "code": return codeComponent;
                                case "tool_call": return toolCallComponent;
                                case "tool_result": return toolResultComponent;
                                case "action_note": return actionNoteComponent;
                                case "command_snippet": return commandSnippetComponent;
                                default: return textComponent;
                            }
                        }
                        property var part: modelData
                    }
                }

                // Fallback for older messages
                Text {
                    visible: messageParts.length === 0 && messageContent !== ""
                    Layout.fillWidth: true
                    text: messageContent
                    color: isUser ? "white" : "#e2e8f0"
                    font.pixelSize: 14 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                    wrapMode: Text.Wrap
                    textFormat: isUser ? Text.RichText : Text.MarkdownText
                }

                // Interactive Suggestions (Buttons)
                Flow {
                    id: suggestionsFlow
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8
                    visible: !isUser && modelData.suggestions !== undefined && modelData.suggestions.length > 0

                    Repeater {
                        model: modelData.suggestions || []
                        delegate: Button {
                            id: sugBtn
                            padding: 8
                            leftPadding: 14; rightPadding: 14
                            
                            background: Rectangle {
                                color: sugBtn.hovered ? "#3b82f6" : "transparent"
                                border.color: sugBtn.hovered ? "#3b82f6" : "#334155"
                                border.width: 1
                                radius: 16
                                Behavior on color { ColorAnimation { duration: 150 } }
                            }

                            contentItem: RowLayout {
                                spacing: 6
                                Text {
                                    text: "✧"
                                    color: sugBtn.hovered ? "white" : "#60a5fa"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Text {
                                    text: modelData.label
                                    color: sugBtn.hovered ? "white" : "#e2e8f0"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }

                            onClicked: {
                                if (typeof geminiBridge !== "undefined") {
                                    geminiBridge.sendMessage(modelData.command)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Part Templates ---
    Component {
        id: textComponent
        Text {
            width: parent ? parent.width : 0
            text: (part && part.content) ? part.content : ""
            color: isUser ? "white" : "#e2e8f0"
            font.family: "Inter, Segoe UI, sans-serif"
            font.pixelSize: 14 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
            wrapMode: Text.Wrap
            textFormat: isUser ? Text.RichText : Text.MarkdownText
            horizontalAlignment: Text.AlignLeft
        }
    }

    Component {
        id: codeComponent
        Rectangle {
            width: parent ? parent.width : 400
            implicitHeight: codeCol.implicitHeight
            radius: 8
            color: "#0f172a"
            border.color: "#1e293b"
            border.width: 1

            ColumnLayout {
                id: codeCol
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true; height: 32; color: "#1e293b"; radius: 8
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8
                        Text {
                            text: part.language.toUpperCase()
                            color: "#94a3b8"
                            font.pixelSize: 10 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                            font.bold: true
                            font.family: "JetBrains Mono"
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: copyBtn
                            flat: true; padding: 4
                            contentItem: Text {
                                text: "Copy"
                                color: copyBtn.hovered ? "white" : "#64748b"
                                font.pixelSize: 10 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                                font.bold: true
                            }
                            onClicked: if (typeof geminiBridge !== "undefined") geminiBridge.copyToClipboard(part.content)
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true; Layout.preferredHeight: Math.min(codeText.implicitHeight + 24, 400); Layout.margins: 12
                    clip: true
                    Text {
                        id: codeText; text: part.content; color: "#f8fafc"
                        font.family: "JetBrains Mono, Monospace"
                        font.pixelSize: 13 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                        wrapMode: Text.NoWrap
                    }
                }
            }
        }
    }

    Component {
        id: toolCallComponent
        Item {
            width: parent ? parent.width : 400
            implicitHeight: callRect.height
            
            Rectangle {
                id: callRect
                width: parent.width
                height: toolCol.implicitHeight + 24
                radius: 8
                color: "#1e293b"
                border.color: "#3b82f6"
                border.width: 1
                property bool expanded: false

                ColumnLayout {
                    id: toolCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        id: callHeader
                        spacing: 8
                        Layout.fillWidth: true
                        
                        Text {
                            text: callRect.expanded ? "▼" : "▶"
                            color: "#3b82f6"
                            font.pixelSize: 10
                        }
                        
                        Text {
                            text: "Calling tool: " + part.name
                            color: "white"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        visible: callRect.expanded
                        Layout.fillWidth: true
                        text: JSON.stringify(part.args, null, 2)
                        color: "#94a3b8"
                        font.family: "JetBrains Mono, Monospace"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
                
                MouseArea {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 40
                    cursorShape: Qt.PointingHandCursor
                    onClicked: callRect.expanded = !callRect.expanded
                }
            }
        }
    }

    Component {
        id: toolResultComponent
        Item {
            width: parent ? parent.width : 400
            implicitHeight: resRect.height

            Rectangle {
                id: resRect
                width: parent.width
                height: resCol.implicitHeight + 24
                radius: 8
                color: Qt.rgba(16, 185, 129, 0.05)
                border.color: Qt.rgba(16, 185, 129, 0.3)
                border.width: 1
                property bool expanded: false

                ColumnLayout {
                    id: resCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        id: resHeader
                        spacing: 8
                        Layout.fillWidth: true
                        
                        Text {
                            text: resRect.expanded ? "▼" : "▶"
                            color: "#10b981"
                            font.pixelSize: 10
                        }
                        
                        Text {
                            text: "Result from " + part.name
                            color: "#10b981"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        visible: resRect.expanded
                        Layout.fillWidth: true
                        text: {
                            if (typeof part.result === "string") return part.result;
                            return JSON.stringify(part.result, null, 2);
                        }
                        color: "#e2e8f0"
                        font.family: "JetBrains Mono, Monospace"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        maximumLineCount: 50
                        elide: Text.ElideRight
                    }
                }
                
                MouseArea {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 40
                    cursorShape: Qt.PointingHandCursor
                    onClicked: resRect.expanded = !resRect.expanded
                }
            }
        }
    }

    Component {
        id: actionNoteComponent
        RowLayout {
            width: parent ? parent.width : 400
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: part.content
                color: "#60a5fa"
                font.italic: true
                font.pixelSize: 12 * (typeof geminiBridge !== "undefined" ? geminiBridge.zoomFactor : 1.0)
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: commandSnippetComponent
        Item {
            width: parent ? parent.width : 400
            implicitHeight: snipRect.height

            Rectangle {
                id: snipRect
                width: parent.width
                height: snipCol.implicitHeight + 24
                radius: 8
                color: "#1e293b"
                border.color: "#f59e0b" // Amber/Yellow for commands
                border.width: 1
                property bool expanded: false

                ColumnLayout {
                    id: snipCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        id: snipHeader
                        spacing: 8
                        Layout.fillWidth: true
                        
                        Text {
                            text: snipRect.expanded ? "▼" : "▶"
                            color: "#f59e0b"
                            font.pixelSize: 10
                        }

                        Text {
                            text: "Executing Editor Commands"
                            color: "#f59e0b"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        visible: snipRect.expanded
                        Layout.fillWidth: true
                        text: {
                            try {
                                var obj = JSON.parse(part.content);
                                if (obj.commands) return obj.commands.join("\n");
                                return part.content;
                            } catch(e) {
                                return part.content;
                            }
                        }
                        color: "#e2e8f0"
                        font.family: "JetBrains Mono, Monospace"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
                
                MouseArea {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 40
                    cursorShape: Qt.PointingHandCursor
                    onClicked: snipRect.expanded = !snipRect.expanded
                }
            }
        }
    }
}
