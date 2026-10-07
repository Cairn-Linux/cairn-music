import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: qsTr("Cairn Music — Pictures Prototype")
    color: "#21152f"

    property bool eraseMode: false
    property int cellWidth: 72
    property int rowHeight: 58
    property int rowGap: 5
    property int totalSteps: app.composition.measureCount * app.composition.stepsPerMeasure
    property var pitchColors: ["#ff6b81", "#ff9f43", "#feca57", "#4cd137",
                               "#38ada9", "#54a0ff", "#a66cff"]
    property var soundColors: ["#ff8f5a", "#ffd166", "#5ee1d2", "#cb8cff"]
    property var soundMarks: ["▥", "◆", "◒", "○"]
    property var soundNames: [qsTr("Keys"), qsTr("Bell"), qsTr("Bird"), qsTr("Bubble")]
    property var drumColors: ["#ff5d8f", "#57c7ff"]
    property var drumMarks: ["●", "✦"]
    property var drumNames: [qsTr("Thump"), qsTr("Clap")]
    property bool placementFeedbackVisible: false
    property bool loopRestartNoticeVisible: false
    readonly property string placementFeedbackMessage: qsTr("Only three sounds can play here.")

    component EraserGlyph: Item {
        width: 32
        height: 28

        Item {
            anchors.centerIn: parent
            width: 29
            height: 15
            rotation: -28

            Rectangle {
                x: 0
                width: 9
                height: parent.height
                radius: 3
                color: "#ff8f9c"
                border.color: "#49324f"
                border.width: 2
            }
            Rectangle {
                x: 7
                width: 22
                height: parent.height
                radius: 3
                color: "#fff0cf"
                border.color: "#49324f"
                border.width: 2
            }
            Rectangle {
                x: 8
                width: 2
                height: parent.height - 4
                anchors.verticalCenter: parent.verticalCenter
                color: "#49324f"
            }
        }
    }

    function placePitchedAt(step, pitch) {
        const placed = app.placePitched(step, pitch)
        if (placed || !app.pitchedPlacementRejected) {
            placementFeedbackVisible = false
        } else {
            placementFeedbackVisible = true
            placementFeedbackTimer.restart()
        }
        return placed
    }

    Timer {
        id: placementFeedbackTimer
        interval: 2200
        repeat: false
        onTriggered: root.placementFeedbackVisible = false
    }

    Timer {
        id: loopRestartNoticeTimer
        interval: 1600
        repeat: false
        onTriggered: root.loopRestartNoticeVisible = false
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: "#2d1b45" }
            GradientStop { position: 1; color: "#151022" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            ColumnLayout {
                spacing: 1
                Label {
                    text: qsTr("PICTURES")
                    color: "#f8cf74"
                    font.pixelSize: 13
                    font.bold: true
                    font.letterSpacing: 2
                }
                Label {
                    text: qsTr("Make a sound. Place a picture. Hear your song.")
                    Layout.maximumWidth: root.width < 1004 ? 400 : 600
                    color: "#fff8ec"
                    font.pixelSize: 25
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                objectName: "undoButton"
                Layout.minimumHeight: 44
                text: qsTr("↶  Undo")
                enabled: app.composition.canUndo
                font.pixelSize: 16
                onClicked: app.undo()
                Accessible.name: qsTr("Undo last change")
            }
            ToolButton {
                id: eraserButton
                objectName: "eraserButton"
                Layout.minimumHeight: 44
                Layout.minimumWidth: 112
                text: qsTr("Eraser")
                checkable: true
                checked: root.eraseMode
                font.pixelSize: 16
                onClicked: root.eraseMode = checked
                Accessible.name: qsTr("Eraser tool")

                contentItem: Row {
                    spacing: 8
                    anchors.centerIn: parent

                    EraserGlyph {
                        id: eraserIcon
                        objectName: "eraserIcon"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: eraserButton.text
                        color: "#fff8ec"
                        font: eraserButton.font
                    }
                }
            }
            Button {
                id: playButton
                objectName: "playButton"
                Layout.minimumHeight: 44
                text: app.playing ? qsTr("■  Stop") : qsTr("▶  Play")
                highlighted: true
                font.pixelSize: 17
                onClicked: app.playing ? app.stop() : app.play()
                Accessible.name: app.playing ? qsTr("Stop song") : qsTr("Play song")
            }
            CheckBox {
                objectName: "loopCheckBox"
                Layout.minimumHeight: 44
                text: qsTr("Loop")
                font.pixelSize: 15
                checked: app.loopEnabled
                onToggled: app.loopEnabled = checked
                Accessible.name: qsTr("Loop whole song")
            }
        }

        Rectangle {
            objectName: "saveFailureBanner"
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            visible: app.saveFailed && !app.loadFailed
            radius: 14
            color: "#fff0c2"
            border.color: "#d69f32"
            border.width: 2
            Accessible.name: app.saveFailureMessage

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                text: app.saveFailureMessage
                color: "#5c3b00"
                font.pixelSize: 17
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            objectName: "audioFailureBanner"
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            visible: app.audioFailed && !app.loadFailed
            radius: 14
            color: "#d9f3ff"
            border.color: "#3b9fc4"
            border.width: 2
            Accessible.name: app.audioFailureMessage
            Accessible.role: Accessible.AlertMessage
            onVisibleChanged: {
                if (visible)
                    Accessible.announce(app.audioFailureMessage, Accessible.Polite)
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                text: app.audioFailureMessage
                color: "#123e52"
                font.pixelSize: 17
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            objectName: "placementFeedback"
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            visible: root.placementFeedbackVisible
            radius: 14
            color: "#efe5ff"
            border.color: "#8b65b3"
            border.width: 2
            Accessible.name: root.placementFeedbackMessage
            Accessible.role: Accessible.AlertMessage
            onVisibleChanged: {
                if (visible)
                    Accessible.announce(root.placementFeedbackMessage, Accessible.Polite)
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                text: root.placementFeedbackMessage
                color: "#3a2948"
                font.pixelSize: 17
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 176
                Layout.fillHeight: true
                radius: 22
                color: "#36254c"
                border.color: "#5b4774"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9

                    Label {
                        text: qsTr("SOUNDS")
                        color: "#d9c8eb"
                        font.pixelSize: 13
                        font.bold: true
                        font.letterSpacing: 1.5
                    }

                    Repeater {
                        model: 4
                        delegate: Button {
                            required property int index
                            objectName: "pitchedSoundButton-%1".arg(index)
                            Layout.fillWidth: true
                            Layout.preferredHeight: 58
                            checkable: true
                            checked: app.selectedKind === "pitched" && app.selectedSound === index
                            onClicked: {
                                root.eraseMode = false
                                app.selectPitched(index)
                            }
                            Accessible.name: root.soundNames[index]

                            contentItem: Row {
                                spacing: 12
                                anchors.centerIn: parent
                                Rectangle {
                                    width: 38
                                    height: 38
                                    radius: 19
                                    color: root.soundColors[index]
                                    Text {
                                        anchors.centerIn: parent
                                        text: root.soundMarks[index]
                                        color: "#21152f"
                                        font.pixelSize: 22
                                        font.bold: true
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.soundNames[index]
                                    color: "#3a2948"
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#5b4774"
                    }

                    Label {
                        text: qsTr("DRUMS")
                        color: "#d9c8eb"
                        font.pixelSize: 13
                        font.bold: true
                        font.letterSpacing: 1.5
                    }

                    Repeater {
                        model: 2
                        delegate: Button {
                            required property int index
                            objectName: "percussionSoundButton-%1".arg(index)
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            checkable: true
                            checked: app.selectedKind === "percussion" && app.selectedSound === index
                            onClicked: {
                                root.eraseMode = false
                                app.selectPercussion(index)
                            }
                            Accessible.name: root.drumNames[index]

                            contentItem: Row {
                                spacing: 12
                                anchors.centerIn: parent
                                Rectangle {
                                    width: 34
                                    height: 34
                                    radius: 9
                                    color: root.drumColors[index]
                                    Text {
                                        anchors.centerIn: parent
                                        text: root.drumMarks[index]
                                        color: "#21152f"
                                        font.pixelSize: 20
                                        font.bold: true
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.drumNames[index]
                                    color: "#3a2948"
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 22
                color: "#fff8ec"
                border.color: "#f0d9ad"
                border.width: 2
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("My song")
                            color: "#3a2948"
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Label {
                            text: qsTr("%1 measures").arg(app.composition.measureCount)
                            color: "#7b668d"
                            font.pixelSize: 14
                        }
                        Label {
                            id: playbackStatus
                            objectName: "playbackStatus"
                            visible: app.playing && text.length > 0
                            text: app.playbackStatus
                            color: "#5b3e73"
                            font.pixelSize: 14
                            font.bold: true
                            Accessible.name: text
                            Accessible.role: Accessible.StaticText

                            Connections {
                                target: app
                                function onPlayingChanged() {
                                    root.loopRestartNoticeVisible = false
                                    loopRestartNoticeTimer.stop()
                                }
                                function onLoopEnabledChanged() {
                                    if (!app.loopEnabled) {
                                        root.loopRestartNoticeVisible = false
                                        loopRestartNoticeTimer.stop()
                                    }
                                }
                                function onPlaybackProgressChanged() {
                                    if (app.playbackStep < 0) {
                                        root.loopRestartNoticeVisible = false
                                        loopRestartNoticeTimer.stop()
                                    }
                                }
                                function onLoopRestarted() {
                                    root.loopRestartNoticeVisible = true
                                    loopRestartNoticeTimer.restart()
                                    playbackStatus.Accessible.announce(
                                        app.playbackStatus, Accessible.Polite)
                                }
                            }
                        }
                        Label {
                            id: loopRestartBadge
                            objectName: "loopRestartBadge"
                            visible: app.playing && root.loopRestartNoticeVisible
                            text: qsTr("Loop %1 • back to beat 1").arg(app.playbackCycle + 1)
                            color: "#3d2452"
                            font.pixelSize: 14
                            font.bold: true
                            Accessible.name: text
                            Accessible.role: Accessible.StaticText
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            objectName: "addMeasureButton"
                            Layout.minimumHeight: 44
                            text: qsTr("＋  Add measure")
                            enabled: app.composition.measureCount < 8
                            onClicked: app.addMeasure()
                            Accessible.name: qsTr("Add one measure")
                        }
                    }

                    Flickable {
                        id: timeline
                        objectName: "timeline"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: Math.max(width, root.totalSteps * root.cellWidth + 20)
                        contentHeight: 7 * (root.rowHeight + root.rowGap) + 150
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        Item {
                            id: canvas
                            width: timeline.contentWidth
                            height: timeline.contentHeight

                            Repeater {
                                model: root.totalSteps
                                delegate: Rectangle {
                                    required property int index
                                    x: index * root.cellWidth
                                    y: 0
                                    width: 2
                                    height: 7 * (root.rowHeight + root.rowGap)
                                    color: index % 4 === 0 ? "#d4b879" : "#eadcc2"
                                    opacity: index % 4 === 0 ? 0.95 : 0.65
                                }
                            }

                            Grid {
                                id: pitchGrid
                                objectName: "pitchGrid"
                                columns: root.totalSteps
                                rows: 7
                                spacing: root.rowGap

                                Repeater {
                                    model: 7 * root.totalSteps
                                    delegate: Rectangle {
                                        required property int index
                                        property int visualRow: Math.floor(index / root.totalSteps)
                                        property int stepIndex: index % root.totalSteps
                                        property int pitch: 6 - visualRow
                                        width: root.cellWidth - root.rowGap
                                        height: root.rowHeight
                                        radius: 12
                                        color: Qt.rgba(root.pitchColors[pitch].r,
                                                       root.pitchColors[pitch].g,
                                                       root.pitchColors[pitch].b, 0.13)
                                        border.color: stepIndex % 4 === 0 ? "#c6a860" : "#decda9"
                                        border.width: stepIndex % 4 === 0 ? 2 : 1

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.leftMargin: 7
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 7
                                            height: 7
                                            radius: 4
                                            color: root.pitchColors[parent.pitch]
                                            opacity: 0.7
                                        }

                                        MouseArea {
                                            objectName: "pitchCellMouse-%1-%2"
                                                .arg(parent.stepIndex).arg(parent.pitch)
                                            anchors.fill: parent
                                            cursorShape: Qt.BlankCursor
                                            onClicked: {
                                                if (root.eraseMode)
                                                    app.eraseAt("pitched", parent.stepIndex, parent.pitch)
                                                else if (app.selectedKind === "pitched")
                                                    root.placePitchedAt(parent.stepIndex, parent.pitch)
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                x: 0
                                y: 7 * (root.rowHeight + root.rowGap) + 12
                                width: parent.width
                                height: 4
                                radius: 2
                                color: "#6e557d"
                                opacity: 0.45
                            }

                            Grid {
                                id: drumLane
                                objectName: "drumLane"
                                x: 0
                                y: 7 * (root.rowHeight + root.rowGap) + 28
                                columns: root.totalSteps
                                rows: 2
                                spacing: root.rowGap

                                Repeater {
                                    model: 2 * root.totalSteps
                                    delegate: Rectangle {
                                        required property int index
                                        property int drumRow: Math.floor(index / root.totalSteps)
                                        property int stepIndex: index % root.totalSteps
                                        width: root.cellWidth - root.rowGap
                                        height: 46
                                        radius: 10
                                        color: Qt.rgba(root.drumColors[drumRow].r,
                                                       root.drumColors[drumRow].g,
                                                       root.drumColors[drumRow].b, 0.15)
                                        border.color: stepIndex % 4 === 0 ? "#9a6c82" : "#d8b8bf"
                                        border.width: stepIndex % 4 === 0 ? 2 : 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.drumMarks[parent.drumRow]
                                            color: root.drumColors[parent.drumRow]
                                            font.pixelSize: 17
                                            opacity: 0.65
                                        }

                                        MouseArea {
                                            objectName: "drumCellMouse-%1-%2"
                                                .arg(parent.stepIndex).arg(parent.drumRow)
                                            anchors.fill: parent
                                            cursorShape: Qt.BlankCursor
                                            onClicked: {
                                                if (root.eraseMode)
                                                    app.eraseAt("percussion", parent.stepIndex,
                                                                parent.drumRow)
                                                else if (app.selectedKind === "percussion"
                                                         && app.selectedSound === parent.drumRow)
                                                    app.placePercussion(parent.stepIndex)
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: playbackPlayhead
                                objectName: "playbackPlayhead"
                                property int currentStep: app.playbackStep
                                x: Math.max(0, currentStep) * root.cellWidth
                                y: 0
                                width: root.cellWidth - root.rowGap
                                height: canvas.height
                                visible: app.playing && currentStep >= 0
                                enabled: false
                                color: "#2b1a3d"
                                opacity: 0.16
                                border.color: "#5b3e73"
                                border.width: 3
                                radius: 10
                                z: 4

                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 4
                                    height: parent.height
                                    color: "#5b3e73"
                                    radius: 2
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 4
                                    text: "▼"
                                    color: "#5b3e73"
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                            }

                            Repeater {
                                model: app.composition
                                delegate: Rectangle {
                                    required property string kind
                                    required property int step
                                    required property int pitchRow
                                    required property int soundId
                                    property bool pitched: kind === "pitched"
                                    property bool sounding: app.playing && app.playbackStep === step
                                    objectName: "compositionToken-%1-%2".arg(step).arg(pitchRow)
                                    x: step * root.cellWidth + 12
                                    y: pitched
                                       ? (6 - pitchRow) * (root.rowHeight + root.rowGap) + 9
                                       : 7 * (root.rowHeight + root.rowGap) + 37
                                           + pitchRow * (46 + root.rowGap)
                                    width: pitched ? 42 : 40
                                    height: pitched ? 42 : 34
                                    radius: pitched ? 21 : 10
                                    color: pitched ? root.soundColors[soundId]
                                                   : root.drumColors[soundId]
                                    border.color: sounding ? "#2b1a3d" : "#ffffff"
                                    border.width: sounding ? 6 : 3
                                    z: 5

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.pitched ? root.soundMarks[parent.soundId]
                                                             : root.drumMarks[parent.soundId]
                                        color: "#21152f"
                                        font.pixelSize: parent.pitched ? 23 : 19
                                        font.bold: true
                                    }

                                    SequentialAnimation on scale {
                                        running: true
                                        NumberAnimation { to: 1.18; duration: 90 }
                                        NumberAnimation { to: 1.0; duration: 150 }
                                    }

                                    MouseArea {
                                        objectName: parent.pitched
                                            ? "pitchedTokenMouse-%1-%2"
                                                  .arg(parent.step).arg(parent.pitchRow)
                                            : ""
                                        anchors.fill: parent
                                        cursorShape: Qt.BlankCursor
                                        onClicked: {
                                            if (root.eraseMode)
                                                app.eraseAt(parent.kind, parent.step, parent.pitchRow)
                                            else if (parent.pitched && app.selectedKind === "pitched")
                                                root.placePitchedAt(parent.step, parent.pitchRow)
                                            else if (!parent.pitched && app.selectedKind === "percussion"
                                                     && app.selectedSound === parent.pitchRow)
                                                app.placePercussion(parent.step)
                                        }
                                    }
                                }
                            }

                            Item {
                                id: compositionHoverSurface
                                x: 0
                                y: 0
                                width: canvas.width
                                height: drumLane.y + drumLane.height
                                z: 90

                                HoverHandler {
                                    id: compositionHover
                                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                    cursorShape: Qt.BlankCursor
                                }
                            }

                            Rectangle {
                                id: activeToolIndicator
                                objectName: "activeToolIndicator"
                                readonly property bool eraserTool: root.eraseMode
                                width: 36
                                height: 36
                                x: Math.min(canvas.width - width,
                                            Math.max(0, compositionHover.point.position.x + 16))
                                y: compositionHover.point.position.y >= height + 12
                                   ? compositionHover.point.position.y - height - 12
                                   : compositionHover.point.position.y + 16
                                visible: compositionHover.hovered
                                enabled: false
                                z: 100
                                radius: eraserTool ? 9
                                                   : app.selectedKind === "pitched" ? 18 : 9
                                color: eraserTool ? "#fff8ec"
                                                  : app.selectedKind === "pitched"
                                                    ? root.soundColors[app.selectedSound]
                                                    : root.drumColors[app.selectedSound]
                                border.color: "#49324f"
                                border.width: 3
                                Accessible.ignored: true

                                Text {
                                    id: activeToolIndicatorMark
                                    objectName: "activeToolIndicatorMark"
                                    anchors.centerIn: parent
                                    visible: !activeToolIndicator.eraserTool
                                    text: app.selectedKind === "pitched"
                                          ? root.soundMarks[app.selectedSound]
                                          : root.drumMarks[app.selectedSound]
                                    color: "#21152f"
                                    font.pixelSize: 21
                                    font.bold: true
                                }

                                EraserGlyph {
                                    anchors.centerIn: parent
                                    visible: activeToolIndicator.eraserTool
                                }
                            }
                        }

                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                    }
                }
            }
        }
    }

    Popup {
        id: loadFailurePopup
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, root.width - 48)
        modal: true
        visible: app.loadFailed
        closePolicy: Popup.NoAutoClose
        padding: 28

        background: Rectangle {
            radius: 20
            color: "#fff8ec"
            border.color: "#f0d9ad"
            border.width: 2
        }

        contentItem: ColumnLayout {
            spacing: 18

            Label {
                Layout.fillWidth: true
                text: app.loadFailureMessage
                color: "#3a2948"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Accessible.name: text
            }

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Keep it safe and start a new song")
                onClicked: app.preserveFailedAutosaveAndStartNew()
                Accessible.name: qsTr("Preserve the old song and start a new song")
            }
        }
    }
}
