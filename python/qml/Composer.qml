import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: composerRoot
// ... (rest of composerRoot)
    width: parent ? parent.width - 40 : 400
    height: Math.max(80, inputColumn.implicitHeight + 32)
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    anchors.bottomMargin: 20
    
    radius: 16
    color: (typeof geminiBridge !== "undefined" && geminiBridge && geminiBridge.glassBackground) ? geminiBridge.glassBackground : "rgba(15, 23, 42, 0.95)"
    border.color: {
        if (typeof geminiBridge === "undefined" || !geminiBridge) return "#334155";
        var m = geminiBridge.currentMode;
        if (m === "Planning") return "#10b981"; // Green
        if (m === "Cmd") return "#f59e0b";      // Yellow
        return "#334155";                      // Normal
    }
    border.width: 1

    signal sendMessage(string text)
    signal stopRun()
    
    // Auto-complete state
    property var fileSuggestions: []
    property int suggestionIndex: 0
    property bool showingSuggestions: fileSuggestions.length > 0
    property int atSymbolPos: -1
    
    // Image state
    property string attachedImage: ""

    function forceFocus() {
        inputField.forceActiveFocus()
    }

    function updateSuggestions() {
        var cursor = inputField.cursorPosition
        var text = inputField.text
        var lastAt = text.lastIndexOf("@", cursor - 1)
        
        if (lastAt !== -1) {
            var query = text.substring(lastAt + 1, cursor)
            if (!query.includes(" ")) {
                atSymbolPos = lastAt
                fileSuggestions = (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.findFiles(query) : []
                suggestionIndex = 0
                return
            }
        }
        
        fileSuggestions = []
        atSymbolPos = -1
    }

    function acceptSuggestion() {
        if (showingSuggestions) {
            var suggestion = fileSuggestions[suggestionIndex]
            var text = inputField.text
            var before = text.substring(0, atSymbolPos + 1)
            var after = text.substring(inputField.cursorPosition)
            inputField.text = before + suggestion + " " + after
            inputField.cursorPosition = before.length + suggestion.length + 1
            fileSuggestions = []
            atSymbolPos = -1
        }
    }

    ColumnLayout {
        id: inputColumn
        width: parent.width - 32
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 12
        spacing: 8

        // Tool Action Bar
        RowLayout {
            id: toolActionBar
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            spacing: 8
            visible: (typeof geminiBridge !== "undefined" && geminiBridge) && geminiBridge.isWorking
            opacity: visible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 250 } }
// ... (rest of toolActionBar)

            // Pulsing dot
            Rectangle {
                width: 6; height: 6; radius: 3
                color: "#10b981"
                Layout.alignment: Qt.AlignVCenter
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.3; duration: 800; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 0.3; to: 1; duration: 800; easing.type: Easing.InOutQuad }
                }
            }

            Text {
                id: toolName
                text: (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.currentTool : "ViorAI"
                color: (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.accentColor : "#3b82f6"
                font.pixelSize: 10 * (typeof geminiBridge !== "undefined" && geminiBridge ? geminiBridge.zoomFactor : 1.0)
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                width: 1; height: 10
                color: Qt.rgba(1, 1, 1, 0.15)
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: actionSubtext
                text: (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.currentAction : ""
                color: "#94a3b8"
                font.pixelSize: 10 * (typeof geminiBridge !== "undefined" && geminiBridge ? geminiBridge.zoomFactor : 1.0)
                font.italic: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // Image Preview Area
        RowLayout {
            visible: attachedImage !== ""
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            spacing: 10
            
            Rectangle {
                width: 48; height: 48; radius: 6
                color: "#1e293b"
                border.color: "#3b82f6"
                clip: true
                
                Image {
                    anchors.fill: parent
                    source: attachedImage !== "" ? "data:image/png;base64," + attachedImage : ""
                    fillMode: Image.PreserveAspectCrop
                }
                
                // Remove button
                Rectangle {
                    anchors.top: parent.top; anchors.right: parent.right
                    anchors.margins: 2
                    width: 14; height: 14; radius: 7
                    color: "#ef4444"
                    Text { anchors.centerIn: parent; text: "×"; color: "white"; font.bold: true; font.pixelSize: 10 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: attachedImage = ""
                    }
                }
            }
            
            Text {
                text: "Image attached"
                color: "#94a3b8"
                font.pixelSize: 11
                Layout.fillWidth: true
            }
        }

        // Input Area
        ScrollView {
            Layout.fillWidth: true
            Layout.maximumHeight: 200
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            clip: true
            
            background: Rectangle {
                color: Qt.rgba(0, 0, 0, 0.2)
                radius: 8
                border.color: Qt.rgba(1, 1, 1, 0.05)
            }

            Item {
                // This container ensures ScrollView calculates content size correctly
                width: parent.width
                implicitHeight: inputField.implicitHeight

                // Highlighting Layer (Shows colors, no interaction)
                Text {
                    id: highlightLayer
                    anchors.fill: inputField
                    anchors.leftMargin: inputField.leftPadding
                    anchors.rightMargin: inputField.rightPadding
                    anchors.topMargin: inputField.topPadding
                    anchors.bottomMargin: inputField.bottomPadding
                    
                    font: inputField.font
                    wrapMode: inputField.wrapMode
                    textFormat: Text.RichText
                    color: "white" // Base text color
                    text: {
                        var raw = inputField.text
                        var escaped = raw.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                        return escaped.replace(/(@\w+[\w\.]*)/g, '<font color="#10b981">$1</font>')
                    }
                    enabled: false
                    z: 1
                }

                TextArea {
                    id: inputField
                    width: parent.width
                    placeholderText: "Message Viora AI..."
                    placeholderTextColor: "#4b5563"
                    color: "transparent"
                    selectedTextColor: "white"
                    selectionColor: "#2563eb"
                    font.pixelSize: 14 * (typeof geminiBridge !== "undefined" && geminiBridge ? geminiBridge.zoomFactor : 1.0)
                    wrapMode: TextArea.Wrap
                    background: null
                    textFormat: TextEdit.PlainText
                    z: 2 // Keep interactive layer on top
                    
                    onTextChanged: {
                        updateSuggestions()
                    }

                    Keys.onPressed: (event) => {
                        // Handle Ctrl+V for image paste
                        if (event.key === Qt.Key_V && (event.modifiers & Qt.ControlModifier)) {
                            var img = (typeof geminiBridge !== "undefined") ? geminiBridge.getImageFromClipboard() : ""
                            if (img !== "") {
                                attachedImage = img
                                event.accepted = true
                                return
                            }
                        }

                        if (showingSuggestions) {
                            if (event.key === Qt.Key_Tab || event.key === Qt.Key_Return) {
                                acceptSuggestion()
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Down) {
                                suggestionIndex = (suggestionIndex + 1) % fileSuggestions.length
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Up) {
                                suggestionIndex = (suggestionIndex - 1 + fileSuggestions.length) % fileSuggestions.length
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Escape) {
                                fileSuggestions = []
                                event.accepted = true
                                return
                            }
                        }

                        if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ControlModifier)) {
                            if (inputField.text.trim() !== "" || attachedImage !== "") {
                                if (attachedImage !== "") {
                                    geminiBridge.sendMessageWithImage(inputField.text.trim(), attachedImage)
                                    attachedImage = ""
                                } else {
                                    composerRoot.sendMessage(inputField.text.trim())
                                }
                                inputField.text = ""
                                inputField.forceActiveFocus()
                            }
                            event.accepted = true
                        }
                    }
                    
                    Component.onCompleted: forceActiveFocus()
                }
            }
        }

        // Action Bar (Redesigned)
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            spacing: 8 // Reduced from 16

            // Left: Utility and Selectors
            RowLayout {
                spacing: 8 // Reduced from 12
                Layout.alignment: Qt.AlignVCenter

                // Mode Selector (with Descriptions)
                MinimalSelector {
                    id: modeSelector
                    Layout.maximumWidth: 100 // Constrain
                    model: (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.availableModes : []
                    currentIndex: {
                        if (typeof geminiBridge === "undefined" || !geminiBridge || !model) return 0;
                        for (var i = 0; i < model.length; i++) {
                            if (model[i].name === geminiBridge.currentMode) return i;
                        }
                        return 0;
                    }
                    onActivated: (index) => { if (typeof geminiBridge !== "undefined" && geminiBridge) geminiBridge.currentMode = model[index].name }
                    
                    textRole: "name"
                    popupWidth: 280
                    showDescriptions: true
                    headerTitle: "Conversation mode"
                }

                // Model Selector
                MinimalSelector {
                    id: modelSelector
                    Layout.maximumWidth: 160 // Constrain more long names
                    model: (typeof geminiBridge !== "undefined" && geminiBridge && geminiBridge.availableModels) ? geminiBridge.availableModels : ["Gemini 2.0 Flash"]
                    currentIndex: (typeof geminiBridge !== "undefined" && geminiBridge && geminiBridge.availableModels) ? Math.max(0, model.indexOf(geminiBridge.currentModel)) : 0
                    onActivated: (index) => { if (typeof geminiBridge !== "undefined" && geminiBridge) geminiBridge.currentModel = model[index] }
                }
            }

            Item { Layout.fillWidth: true; Layout.minimumWidth: 4 } // Adaptive spacer

            // Right: Secondary Actions and Primary Round Button
            RowLayout {
                spacing: 8 // Reduced from 12
                Layout.alignment: Qt.AlignVCenter

                IconButton {
                    iconSource: "qrc:/icons/tool_oscilloscope.svg"
                    ToolTip.text: "Voice Input (Placeholder)"
                }

                // Primary Action Button (Circular)
                Button {
                    id: mainActionBtn
                    property bool isWorking: (typeof geminiBridge !== "undefined" && geminiBridge) ? geminiBridge.isWorking : false
                    
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    
                    onClicked: {
                        if (isWorking) {
                            composerRoot.stopRun()
                        } else if (inputField.text.trim() !== "" || attachedImage !== "") {
                            if (attachedImage !== "") {
                                geminiBridge.sendMessageWithImage(inputField.text.trim(), attachedImage)
                                attachedImage = ""
                            } else {
                                composerRoot.sendMessage(inputField.text.trim())
                            }
                            inputField.text = ""
                            inputField.forceActiveFocus()
                        }
                    }

                    background: Rectangle {
                        radius: 16
                        color: mainActionBtn.isWorking ? "#ef4444" : (mainActionBtn.enabled ? "#2563eb" : "#1e293b")
                        
                        // Icon Content
                        Text {
                            anchors.centerIn: parent
                            text: mainActionBtn.isWorking ? "■" : "→"
                            color: "white"
                            font.pixelSize: mainActionBtn.isWorking ? 10 : 16
                            font.bold: true
                        }
                    }
                    
                    enabled: isWorking || inputField.text.trim() !== ""
                }
            }
        }
    }

    // --- Sub-Components ---

    // Minimal IconButton
    component IconButton : Button {
        property string iconSource: ""
        
        id: iconBtn
        flat: true
        font.pixelSize: 18
        
        background: Rectangle {
            implicitWidth: 28
            implicitHeight: 28
            color: iconBtn.hovered ? "rgba(255, 255, 255, 0.08)" : "transparent"
            radius: 6
        }
        
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                source: iconBtn.iconSource
                width: 16; height: 16
                visible: iconBtn.iconSource !== ""
                cache: true
                asynchronous: true
                layer.enabled: true
                layer.effect: ColorOverlay { color: iconBtn.hovered ? "white" : "#94a3b8" }
            }

            Text {
                anchors.centerIn: parent
                text: iconBtn.text
                color: iconBtn.hovered ? "white" : "#94a3b8"
                font: iconBtn.font
                visible: iconBtn.iconSource === ""
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 500
    }

    // Minimal minimalist dropdown selector
    component MinimalSelector : ComboBox {
        id: cb
        
        property int popupWidth: 200
        property bool showDescriptions: false
        property string headerTitle: ""

        background: Item { 
            implicitWidth: contentLayout.implicitWidth + 8
            implicitHeight: 28
        }
        
        contentItem: RowLayout {
            id: contentLayout
            spacing: 4
            
            Text {
                text: "^" 
                color: "#94a3b8"
                font.pixelSize: 10
                font.bold: true
                rotation: cb.popup.visible ? 0 : 180 // Toggle orientation
                Layout.alignment: Qt.AlignVCenter
            }
            
            Text {
                text: cb.currentText
                color: cb.hovered ? "white" : "#94a3b8"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: 120
            }
        }

        delegate: ItemDelegate {
            id: delegateRoot
            width: cb.popupWidth
            padding: 12
            
            contentItem: Column {
                spacing: 4
                Text {
                    text: (typeof modelData === "object") ? modelData.name : modelData
                    color: highlighted || hovered ? "white" : "#e2e8f0"
                    font.pixelSize: 12 * (typeof geminiBridge !== "undefined" && geminiBridge ? geminiBridge.zoomFactor : 1.0)
                    font.bold: true
                }
                Text {
                    visible: cb.showDescriptions && (typeof modelData === "object") && modelData.desc
                    text: (typeof modelData === "object") ? modelData.desc : ""
                    color: "#94a3b8"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                    width: parent.width
                }
            }
            background: Rectangle {
                color: highlighted ? "rgba(37, 99, 235, 0.15)" : (hovered ? "rgba(255, 255, 255, 0.05)" : "transparent")
                border.color: highlighted ? "#2563eb" : "transparent"
                border.width: 1
                radius: 8
            }
        }

        popup: Popup {
            onClosed: composerRoot.forceFocus()
            y: -height - 8
            width: cb.popupWidth
            implicitHeight: contentColumn.implicitHeight + (cb.headerTitle !== "" ? 40 : 0)
            padding: 8
            
            contentItem: Column {
                id: contentColumn
                spacing: 8
                
                Text {
                    visible: cb.headerTitle !== ""
                    text: cb.headerTitle
                    color: "#64748b"
                    font.pixelSize: 11
                    font.bold: true
                    padding: 4
                }
                
                ListView {
                    id: listView
                    clip: true
                    implicitHeight: contentHeight
                    width: parent.width
                    model: cb.popup.visible ? cb.delegateModel : null
                    ScrollIndicator.vertical: ScrollIndicator { }
                }
            }
            
            background: Rectangle {
                color: "#1e293b"
                border.color: "#334155"
                radius: 12
                
                // Subtle shadow
                layer.enabled: true
                layer.effect: ShaderEffect { } 
                // Using layer for standard shadows is complex without generic libs, 
                // so I'll just use a slightly darker border.
            }
        }
    }

    // Suggestions Popup (Absolute Overlay)
    Rectangle {
        id: suggestionPopup
        visible: showingSuggestions
        width: Math.min(300, composerRoot.width - 40)
        height: Math.min(200, suggestionList.contentHeight + 16)
        color: "#1e293b"
        border.color: "#3b82f6"
        border.width: 1
        radius: 8
        z: 1000 // Ensure it's on top of everything
        
        // Position it above the composer
        anchors.bottom: composerRoot.top
        anchors.bottomMargin: 8
        anchors.left: composerRoot.left
        anchors.leftMargin: 20

        ListView {
            id: suggestionList
            anchors.fill: parent
            anchors.margins: 8
            model: fileSuggestions
            clip: true
            
            delegate: Rectangle {
                width: suggestionList.width
                height: 32
                color: index === suggestionIndex ? "rgba(59, 130, 246, 0.2)" : "transparent"
                radius: 4
                
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    verticalAlignment: Text.AlignHCenter
                    text: modelData
                    color: index === suggestionIndex ? "white" : "#94a3b8"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                }
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        suggestionIndex = index
                        acceptSuggestion()
                    }
                }
            }
        }
    }
}
