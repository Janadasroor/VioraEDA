import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    implicitWidth: 400
    implicitHeight: 600
    color: "#0a0f18" // Deep base background

    // Top Navigation/Header
    PanelHeader {
        id: header
        anchors.top: parent.top
        z: 10
        onShowDashboardRequested: dashboard.visible = !dashboard.visible
        onRequestInputFocus: composer.forceFocus()
    }

    // Main Chat History
    ListView {
        id: chatListView
        width: parent.width
        anchors.top: header.bottom
        anchors.bottom: composer.top
        anchors.topMargin: 4
        anchors.bottomMargin: 4
        spacing: 8
        clip: true
        
        model: (geminiBridge && geminiBridge.messages) ? geminiBridge.messages : []
        
        delegate: ChatCard {
            width: chatListView.width
            onShowDashboardRequested: dashboard.visible = true
        }
        
        // Auto-scroll to bottom on new items
        onCountChanged: {
            if (count > 0) {
                positionViewAtEnd();
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            active: true
        }

        // Global Zoom (Ctrl + Wheel)
        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => {
                if (event.angleDelta.y > 0) {
                    if (typeof geminiBridge !== "undefined" && geminiBridge) geminiBridge.zoomIn()
                } else {
                    if (typeof geminiBridge !== "undefined" && geminiBridge) geminiBridge.zoomOut()
                }
            }
        }
    }

    // Floating Composer (as designed in Composer.qml)
    Composer {
        id: composer
        onSendMessage: (msg) => geminiBridge.sendMessage(msg)
        onStopRun: geminiBridge.stopRun()
    }

    // Tool Activity Dashboard (Overlay)
    ToolDashboard {
        id: dashboard
        z: 20
        anchors.fill: parent
        anchors.margins: 20
        visible: false
    }
}
