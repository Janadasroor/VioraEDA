import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 400
    height: 600
    color: "#0a0f18" // Deep base background

    // Top Navigation/Header
    PanelHeader {
        id: header
        anchors.top: parent.top
        z: 10
    }

    // Main Chat History
    ListView {
        id: chatListView
        width: parent.width
        anchors.top: header.bottom
        anchors.bottom: composer.top
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        spacing: 15
        clip: true
        
        model: (geminiBridge && geminiBridge.messages) ? geminiBridge.messages : []
        
        delegate: ChatCard {
            width: chatListView.width
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
    }

    // Floating Composer (as designed in Composer.qml)
    Composer {
        id: composer
        onSendMessage: (msg) => geminiBridge.sendMessage(msg)
        onStopRun: geminiBridge.stopRun()
    }

    // Initial load
    Component.onCompleted: {
        updateMessages();
    }
}
