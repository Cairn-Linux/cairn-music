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
    property bool reduceMotion: false
    readonly property bool motionEnabled: !reduceMotion
    property bool hasInteracted: false
    property bool inactivityHintVisible: false
    property bool focusInteractionArmed: false
    property int cellWidth: 72
    readonly property bool compactLayout: height < 760
    // Qt 6.11 has no system motion-preference API. Keep one truthful in-app
    // policy for every animation; Qt 6.12+ can add platform preference here.
    readonly property bool placementAnimationsEnabled: motionEnabled && !compactLayout
    property int rowHeight: compactLayout ? 44 : 52
    property int rowGap: compactLayout ? 2 : 4
    property int drumRowHeight: 44
    readonly property int pitchLaneHeight: 7 * rowHeight + 6 * rowGap
    readonly property int drumLaneTop: pitchLaneHeight + (compactLayout ? 6 : 20)
    readonly property int compositionHeight:
        drumLaneTop + 2 * drumRowHeight + rowGap + (compactLayout ? 4 : 8)
    property int totalSteps: app.composition.measureCount * app.composition.stepsPerMeasure
    property var pitchColors: ["#ff6b81", "#ff9f43", "#feca57", "#4cd137",
                               "#38ada9", "#54a0ff", "#a66cff"]
    property var soundColors: ["#ff8f5a", "#ffd166", "#5ee1d2", "#cb8cff"]
    property var soundMarks: ["▥", "◆", "◒", "○"]
    property var soundNames: [qsTr("Keys"), qsTr("Pluck"), qsTr("Bell"), qsTr("Bubble")]
    property var drumColors: ["#ff5d8f", "#57c7ff"]
    property var drumMarks: ["●", "✦"]
    property var drumNames: [qsTr("Thump"), qsTr("Clap")]
    property bool placementFeedbackVisible: false
    property string placementFeedbackMessage: qsTr("Only three sounds can play here.")
    property bool loopRestartNoticeVisible: false
    property bool recoveryActionFailed: false
    readonly property color panelColor: "#36254c"
    readonly property color panelTextColor: "#fff8ec"
    readonly property color panelHoverColor: "#f0d9ad"
    readonly property color panelFocusColor: "#f8cf74"
    readonly property color panelDisabledColor: "#b9a9c8"
    readonly property bool modalPopupActive: clearSongDialog.visible || loadFailurePopup.visible
    property int timelineFocusStep: 0
    property int timelineFocusLane: 0
    readonly property bool toolPointerActive:
        active && Qt.application.state === Qt.ApplicationActive

    onActiveFocusItemChanged: {
        if (focusInteractionArmed && activeFocusItem)
            markInteraction()
    }


    component EraserGlyph: Item {
        id: eraserGlyphRoot
        property bool selected: false
        property string selectionMarkObjectName: ""
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

        Rectangle {
            objectName: eraserGlyphRoot.selectionMarkObjectName
            anchors.top: parent.top
            anchors.right: parent.right
            width: 16
            height: 16
            radius: 8
            visible: eraserGlyphRoot.selected
            color: "#21152f"
            border.color: "#fff8ec"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "✓"
                color: "#fff8ec"
                font.pixelSize: 12
                font.bold: true
                Accessible.ignored: true
            }
        }
    }

    function placePitchedAt(step, pitch) {
        const placed = app.placePitched(step, pitch)
        if (placed || !app.pitchedPlacementRejected) {
            clearPlacementFeedback()
        } else {
            showPlacementFeedback(qsTr("Only three sounds can play here."))
        }
        return placed
    }

    function markInteraction() {
        hasInteracted = true
        inactivityHintVisible = false
        if (modalPopupActive)
            inactivityTimer.stop()
        else
            inactivityTimer.restart()
    }

    function showInactivityGuidance() {
        if (!modalPopupActive && !hasInteracted)
            inactivityHintVisible = true
    }

    function suspendInactivityGuidance() {
        inactivityTimer.stop()
        inactivityHintVisible = false
    }

    function beginInactivityCycle() {
        inactivityHintVisible = false
        if (!modalPopupActive) {
            hasInteracted = true
            inactivityTimer.restart()
        }
    }

    function handleInactivityTimeout() {
        hasInteracted = false
        showInactivityGuidance()
    }

    onModalPopupActiveChanged: {
        if (modalPopupActive)
            suspendInactivityGuidance()
        else
            beginInactivityCycle()
    }

    function findTimelineDescendant(item, wantedName) {
        if (!item)
            return null
        if (item.objectName === wantedName)
            return item
        const descendants = item.children
        for (let index = 0; index < descendants.length; ++index) {
            const match = findTimelineDescendant(descendants[index], wantedName)
            if (match)
                return match
        }
        return null
    }

    // The timeline is one Tab stop. Arrow keys move through its spatial grid;
    // a placed token replaces its cell as the focus target instead of adding an
    // insertion-ordered stop. Tab/Backtab leave to the surrounding controls.
    function timelineTarget(step, lane) {
        let tokenName
        let cellName
        if (lane < 7) {
            const pitch = 6 - lane
            tokenName = "pitchedTokenMouse-%1-%2".arg(step).arg(pitch)
            cellName = "pitchCellMouse-%1-%2".arg(step).arg(pitch)
        } else {
            const drum = lane - 7
            tokenName = "percussionTokenMouse-%1-%2".arg(step).arg(drum)
            cellName = "drumCellMouse-%1-%2".arg(step).arg(drum)
        }
        return findTimelineDescendant(canvas, tokenName)
            || findTimelineDescendant(canvas, cellName)
    }

    function focusTimelinePosition(step, lane) {
        timelineFocusStep = Math.max(0, Math.min(totalSteps - 1, step))
        timelineFocusLane = Math.max(0, Math.min(8, lane))
        const target = timelineTarget(timelineFocusStep, timelineFocusLane)
        if (target) {
            target.forceActiveFocus(Qt.TabFocusReason)
            revealTimelineItem(target)
        }
    }

    function handleTimelineKey(event, step, lane) {
        if (modalPopupActive)
            return
        if (event.key === Qt.Key_Left) {
            focusTimelinePosition(step - 1, lane)
        } else if (event.key === Qt.Key_Right) {
            focusTimelinePosition(step + 1, lane)
        } else if (event.key === Qt.Key_Up) {
            focusTimelinePosition(step, lane - 1)
        } else if (event.key === Qt.Key_Down) {
            focusTimelinePosition(step, lane + 1)
        } else if (event.key === Qt.Key_Tab && !(event.modifiers & Qt.ShiftModifier)) {
            const next = undoButton.enabled ? undoButton : eraserButton
            next.forceActiveFocus(Qt.TabFocusReason)
        } else if (event.key === Qt.Key_Backtab
                   || (event.key === Qt.Key_Tab && event.modifiers & Qt.ShiftModifier)) {
            const previous = addMeasureButton.enabled ? addMeasureButton : removeMeasureButton
            previous.forceActiveFocus(Qt.BacktabFocusReason)
        } else {
            return
        }
        event.accepted = true
    }

    function activatePitchCell(step, pitch) {
        markInteraction()
        if (eraseMode) {
            const restoreFocus = activeFocusItem
                && activeFocusItem.objectName
                    === "pitchedTokenMouse-%1-%2".arg(step).arg(pitch)
            app.eraseAt("pitched", step, pitch)
            if (restoreFocus)
                Qt.callLater(focusTimelinePosition, step, 6 - pitch)
        } else if (app.selectedKind === "pitched")
            placePitchedAt(step, pitch)
        else if (app.selectedKind === "percussion")
            showDrumPlacementGuidance()
    }

    function activateDrumCell(step, drumRow) {
        markInteraction()
        if (eraseMode) {
            const restoreFocus = activeFocusItem
                && activeFocusItem.objectName
                    === "percussionTokenMouse-%1-%2".arg(step).arg(drumRow)
            app.eraseAt("percussion", step, drumRow)
            if (restoreFocus)
                Qt.callLater(focusTimelinePosition, step, 7 + drumRow)
        } else if (app.selectedKind === "percussion" && app.selectedSound === drumRow) {
            clearPlacementFeedback()
            app.placePercussion(step)
        } else if (app.selectedKind === "percussion") {
            showDrumPlacementGuidance()
        }
    }

    function revealTimelineItem(item) {
        const position = item.mapToItem(canvas, 0, 0)
        const padding = 4
        const left = position.x - padding
        const right = position.x + item.width + padding
        const top = position.y - padding
        const bottom = position.y + item.height + padding
        const maximumX = Math.max(0, timeline.contentWidth - timeline.width)
        const maximumY = Math.max(0, timeline.contentHeight - timeline.height)
        let nextX = timeline.contentX
        let nextY = timeline.contentY
        if (left < nextX)
            nextX = left
        else if (right > nextX + timeline.width)
            nextX = right - timeline.width
        if (top < nextY)
            nextY = top
        else if (bottom > nextY + timeline.height)
            nextY = bottom - timeline.height
        timeline.contentX = Math.max(0, Math.min(maximumX, nextX))
        timeline.contentY = Math.max(0, Math.min(maximumY, nextY))
    }

    function showPlacementFeedback(message) {
        placementFeedbackMessage = message
        placementFeedbackVisible = true
        placementFeedbackTimer.restart()
    }

    function clearPlacementFeedback() {
        placementFeedbackVisible = false
        placementFeedbackTimer.stop()
    }

    function showDrumPlacementGuidance() {
        const drumName = drumNames[app.selectedSound]
        showPlacementFeedback(qsTr("Put %1 in the %1 drum row.").arg(drumName))
    }

    Timer {
        id: placementFeedbackTimer
        interval: 2200
        repeat: false
        onTriggered: root.placementFeedbackVisible = false
    }

    Timer {
        id: inactivityTimer
        objectName: "inactivityTimer"
        interval: 6000
        repeat: false
        onTriggered: root.handleInactivityTimeout()
    }

    Timer {
        id: focusInteractionArmTimer
        interval: 250
        repeat: false
        running: true
        onTriggered: root.focusInteractionArmed = true
    }

    Component.onCompleted: beginInactivityCycle()

    Timer {
        id: loopRestartNoticeTimer
        interval: 1600
        repeat: false
        onTriggered: root.loopRestartNoticeVisible = false
    }

    Dialog {
        id: clearSongDialog
        objectName: "clearSongDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, root.width - 48)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Clear this song?")
        onOpened: cancelClearSongButton.forceActiveFocus(Qt.PopupFocusReason)

        contentItem: Label {
            objectName: "clearSongMessage"
            text: qsTr("The current song will be cleared.")
            color: "#3a2948"
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            Accessible.name: text
            Accessible.role: Accessible.StaticText
        }

        footer: DialogButtonBox {
            Button {
                id: cancelClearSongButton
                objectName: "cancelClearSongButton"
                implicitHeight: 44
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                Accessible.name: qsTr("Cancel clearing song")
                onClicked: root.markInteraction()
            }
            Button {
                objectName: "confirmClearSongButton"
                implicitHeight: 44
                text: qsTr("Clear Song")
                highlighted: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                Accessible.name: qsTr("Confirm Clear Song")
                onClicked: root.markInteraction()
            }
            onRejected: clearSongDialog.reject()
            onAccepted: {
                clearSongDialog.accept()
                app.clearSong()
            }
        }
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
        anchors.bottomMargin: root.compactLayout ? 4 : 24
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: root.width < 1280 ? 4 : 14

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
                    Layout.maximumWidth: root.width < 1004 ? 300
                        : root.width < 1280 ? 480 : 600
                    color: "#fff8ec"
                    font.pixelSize: root.width < 1004 ? 20 : 25
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                id: undoButton
                objectName: "undoButton"
                readonly property color normalBackgroundColor: "#46325d"
                readonly property color hoverBackgroundColor: "#5b4774"
                readonly property color disabledBackgroundColor: "#2b2139"
                readonly property color currentBackgroundColor: !enabled
                    ? disabledBackgroundColor : hovered ? hoverBackgroundColor : normalBackgroundColor
                readonly property color currentTextColor: enabled
                    ? root.panelTextColor : root.panelDisabledColor
                Layout.minimumHeight: 44
                Layout.minimumWidth: 100
                text: qsTr("↶  Undo")
                enabled: app.composition.canUndo
                font.pixelSize: 18
                font.bold: true
                onClicked: {
                    root.markInteraction()
                    app.undo()
                }
                Accessible.name: qsTr("Undo last change")

                background: Rectangle {
                    id: undoBackground
                    objectName: "undoBackground"
                    radius: 12
                    color: undoButton.currentBackgroundColor
                    border.color: undoButton.activeFocus ? root.panelFocusColor : "#6e5a83"
                    border.width: undoButton.activeFocus ? 3 : 1
                }

                contentItem: Text {
                    id: undoLabel
                    objectName: "undoLabel"
                    text: undoButton.text
                    color: undoButton.currentTextColor
                    font: undoButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            ToolButton {
                id: eraserButton
                objectName: "eraserButton"
                readonly property color selectedBackgroundColor: root.panelFocusColor
                readonly property color normalBackgroundColor: "#46325d"
                readonly property color hoverBackgroundColor: "#5b4774"
                readonly property color selectedBorderColor: "#21152f"
                readonly property color currentBackgroundColor: checked ? selectedBackgroundColor
                    : hovered ? hoverBackgroundColor : normalBackgroundColor
                readonly property color currentTextColor: checked ? "#21152f" : root.panelTextColor
                readonly property color currentBorderColor: activeFocus
                    ? (checked ? selectedBorderColor : root.panelFocusColor)
                    : checked ? selectedBorderColor : "#6e5a83"
                Layout.minimumHeight: 44
                Layout.minimumWidth: 112
                text: qsTr("Eraser")
                checkable: true
                checked: root.eraseMode
                font.pixelSize: 18
                font.bold: true
                onClicked: {
                    root.markInteraction()
                    root.eraseMode = checked
                }
                Accessible.name: qsTr("Eraser tool")

                background: Rectangle {
                    id: eraserBackground
                    objectName: "eraserBackground"
                    radius: 12
                    color: eraserButton.currentBackgroundColor
                    border.color: eraserButton.currentBorderColor
                    border.width: eraserButton.activeFocus ? 3 : eraserButton.checked ? 2 : 1
                }

                contentItem: Row {
                    spacing: 8
                    anchors.centerIn: parent

                    EraserGlyph {
                        id: eraserIcon
                        objectName: "eraserIcon"
                        selected: eraserButton.checked
                        selectionMarkObjectName: "eraserMark"
                    }

                    Text {
                        id: eraserLabel
                        objectName: "eraserLabel"
                        anchors.verticalCenter: parent.verticalCenter
                        text: eraserButton.text
                        color: eraserButton.currentTextColor
                        font: eraserButton.font
                    }
                }
            }
            Button {
                id: playButton
                objectName: "playButton"
                Layout.minimumHeight: 44
                text: app.compositionPlaying ? qsTr("■  Stop") : qsTr("▶  Play")
                highlighted: true
                font.pixelSize: 17
                onClicked: {
                    root.markInteraction()
                    app.compositionPlaying ? app.stop() : app.play()
                }
                Accessible.name: app.compositionPlaying ? qsTr("Stop song") : qsTr("Play song")
            }
            CheckBox {
                id: loopCheckBox
                objectName: "loopCheckBox"
                readonly property color selectedBackgroundColor: root.soundColors[2]
                readonly property color normalBackgroundColor: "#46325d"
                readonly property color hoverBackgroundColor: "#5b4774"
                readonly property color selectedBorderColor: "#21152f"
                readonly property color currentBackgroundColor: checked ? selectedBackgroundColor
                    : hovered ? hoverBackgroundColor : normalBackgroundColor
                readonly property color currentTextColor: checked ? "#21152f" : root.panelTextColor
                readonly property color currentBorderColor: activeFocus
                    ? (checked ? selectedBorderColor : root.panelFocusColor)
                    : checked ? selectedBorderColor : "#6e5a83"
                Layout.minimumHeight: 44
                Layout.minimumWidth: 100
                text: qsTr("Loop")
                font.pixelSize: 18
                font.bold: true
                checked: app.loopEnabled
                onToggled: {
                    root.markInteraction()
                    app.loopEnabled = checked
                }
                Accessible.name: qsTr("Loop whole song")

                background: Rectangle {
                    id: loopBackground
                    objectName: "loopBackground"
                    radius: 12
                    color: loopCheckBox.currentBackgroundColor
                    border.color: loopCheckBox.currentBorderColor
                    border.width: loopCheckBox.activeFocus ? 3 : loopCheckBox.checked ? 2 : 1
                }

                indicator: Rectangle {
                    id: loopIndicator
                    objectName: "loopIndicator"
                    implicitWidth: 26
                    implicitHeight: 26
                    x: loopCheckBox.leftPadding
                    y: (loopCheckBox.height - height) / 2
                    radius: 7
                    color: loopCheckBox.checked ? "#fff8ec" : "transparent"
                    border.color: loopCheckBox.checked ? "#21152f" : root.panelTextColor
                    border.width: 3

                    Text {
                        id: loopMark
                        objectName: "loopMark"
                        anchors.centerIn: parent
                        visible: loopCheckBox.checked
                        text: "✓"
                        color: "#21152f"
                        font.pixelSize: 19
                        font.bold: true
                        Accessible.ignored: true
                    }
                }

                contentItem: Text {
                    id: loopLabel
                    objectName: "loopLabel"
                    leftPadding: loopCheckBox.indicator.width + loopCheckBox.spacing
                    text: loopCheckBox.text
                    color: loopCheckBox.currentTextColor
                    font: loopCheckBox.font
                    verticalAlignment: Text.AlignVCenter
                }
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
                            id: pitchedSoundButton
                            required property int index
                            objectName: "pitchedSoundButton-%1".arg(index)
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44
                            Layout.preferredHeight: root.compactLayout ? 50 : 58
                            checkable: true
                            checked: app.selectedKind === "pitched" && app.selectedSound === index
                            onClicked: {
                                root.markInteraction()
                                root.eraseMode = false
                                app.selectPitched(index)
                            }
                            Accessible.name: root.soundNames[index]

                            contentItem: Row {
                                spacing: 10
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
                                Text {
                                    objectName: "pitchedSelectionMark-%1".arg(index)
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: pitchedSoundButton.checked
                                    text: "✓"
                                    color: "#3a2948"
                                    font.pixelSize: 20
                                    font.bold: true
                                    Accessible.ignored: true
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
                            id: percussionSoundButton
                            required property int index
                            objectName: "percussionSoundButton-%1".arg(index)
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44
                            Layout.preferredHeight: root.compactLayout ? 46 : 52
                            checkable: true
                            checked: app.selectedKind === "percussion" && app.selectedSound === index
                            onClicked: {
                                root.markInteraction()
                                root.eraseMode = false
                                app.selectPercussion(index)
                            }
                            Accessible.name: root.drumNames[index]

                            contentItem: Row {
                                spacing: 10
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
                                Text {
                                    objectName: "percussionSelectionMark-%1".arg(index)
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: percussionSoundButton.checked
                                    text: "✓"
                                    color: "#3a2948"
                                    font.pixelSize: 20
                                    font.bold: true
                                    Accessible.ignored: true
                                }
                            }
                        }
                    }

                    CheckBox {
                        id: reduceMotionCheckBox
                        objectName: "reduceMotionCheckBox"
                        readonly property color normalTextColor: root.panelTextColor
                        readonly property color hoverTextColor: root.panelHoverColor
                        readonly property color focusTextColor: root.panelFocusColor
                        readonly property color checkedTextColor: root.panelFocusColor
                        readonly property color disabledTextColor: root.panelDisabledColor
                        readonly property color normalIndicatorColor: root.panelTextColor
                        readonly property color hoverIndicatorColor: root.panelHoverColor
                        readonly property color focusIndicatorColor: root.panelFocusColor
                        readonly property color checkedIndicatorColor: root.panelFocusColor
                        readonly property color disabledIndicatorColor: root.panelDisabledColor
                        readonly property color currentTextColor: !enabled ? disabledTextColor
                            : activeFocus ? focusTextColor
                            : checked ? checkedTextColor
                            : hovered ? hoverTextColor : normalTextColor
                        readonly property color currentIndicatorColor: !enabled
                            ? disabledIndicatorColor
                            : activeFocus ? focusIndicatorColor
                            : checked ? checkedIndicatorColor
                            : hovered ? hoverIndicatorColor : normalIndicatorColor
                        Layout.fillWidth: true
                        Layout.minimumHeight: 44
                        text: qsTr("Reduce motion")
                        font.pixelSize: 14
                        checked: root.reduceMotion
                        onClicked: {
                            root.markInteraction()
                            root.reduceMotion = checked
                        }
                        Accessible.name: qsTr("Reduce motion")

                        indicator: Rectangle {
                            id: reduceMotionIndicator
                            objectName: "reduceMotionIndicator"
                            implicitWidth: 28
                            implicitHeight: 28
                            x: reduceMotionCheckBox.leftPadding
                            y: (reduceMotionCheckBox.height - height) / 2
                            radius: 6
                            color: "transparent"
                            border.color: reduceMotionCheckBox.currentIndicatorColor
                            border.width: reduceMotionCheckBox.activeFocus ? 4 : 3

                            Text {
                                id: reduceMotionMark
                                objectName: "reduceMotionMark"
                                anchors.centerIn: parent
                                visible: reduceMotionCheckBox.checked
                                text: "✓"
                                color: reduceMotionCheckBox.currentIndicatorColor
                                font.pixelSize: 21
                                font.bold: true
                                Accessible.ignored: true
                            }
                        }

                        contentItem: Text {
                            id: reduceMotionLabel
                            objectName: "reduceMotionLabel"
                            leftPadding: reduceMotionCheckBox.indicator.width
                                + reduceMotionCheckBox.spacing
                            text: reduceMotionCheckBox.text
                            color: reduceMotionCheckBox.currentTextColor
                            font: reduceMotionCheckBox.font
                            verticalAlignment: Text.AlignVCenter
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
                    anchors.margins: root.compactLayout ? 6 : 16
                    spacing: root.compactLayout ? 4 : 10

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
                            visible: app.compositionPlaying && text.length > 0
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
                            visible: app.compositionPlaying && root.loopRestartNoticeVisible
                            text: qsTr("Loop %1 • back to beat 1").arg(app.playbackCycle + 1)
                            color: "#3d2452"
                            font.pixelSize: 14
                            font.bold: true
                            Accessible.name: text
                            Accessible.role: Accessible.StaticText
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: clearSongButton
                            objectName: "clearSongButton"
                            Layout.minimumWidth: 44
                            Layout.minimumHeight: 44
                            text: qsTr("Clear Song")
                            onClicked: {
                                root.markInteraction()
                                clearSongDialog.open()
                            }
                            Accessible.name: qsTr("Clear Song")
                        }
                        Button {
                            id: removeMeasureButton
                            objectName: "removeMeasureButton"
                            Layout.minimumWidth: 44
                            Layout.minimumHeight: 44
                            text: qsTr("−  Remove measure")
                            enabled: app.composition.measureCount > 2
                            onClicked: {
                                root.markInteraction()
                                app.removeMeasure()
                            }
                            Accessible.name: qsTr("Remove final measure")
                        }
                        Button {
                            id: addMeasureButton
                            objectName: "addMeasureButton"
                            Layout.minimumWidth: 44
                            Layout.minimumHeight: 44
                            text: qsTr("＋  Add measure")
                            enabled: app.composition.measureCount < 8
                            onClicked: {
                                root.markInteraction()
                                app.addMeasure()
                            }
                            Accessible.name: qsTr("Add one measure")
                        }
                    }

                    Flickable {
                        id: timeline
                        objectName: "timeline"
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Pane
                        Accessible.name: qsTr("Song timeline. Use arrow keys to move between beats and rows.")
                        onActiveFocusChanged: {
                            if (activeFocus && root.activeFocusItem === timeline)
                                Qt.callLater(root.focusTimelinePosition,
                                             root.timelineFocusStep,
                                             root.timelineFocusLane)
                        }
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: Math.max(width, root.totalSteps * root.cellWidth + 20)
                        contentHeight: root.compositionHeight
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
                                    height: root.pitchLaneHeight
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
                                        border.color: pitchCellMouse.activeFocus
                                            ? "#2b1a3d"
                                            : stepIndex % 4 === 0 ? "#c6a860" : "#decda9"
                                        border.width: pitchCellMouse.activeFocus
                                            ? 4 : stepIndex % 4 === 0 ? 2 : 1

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

                                        Rectangle {
                                            id: inactivityHint
                                            objectName: parent.stepIndex === 0 && parent.pitch === 3
                                                ? "inactivityHint" : ""
                                            property real animatedOpacity: 0.45
                                            anchors.fill: parent
                                            anchors.margins: 3
                                            visible: root.inactivityHintVisible
                                                && parent.stepIndex === 0 && parent.pitch === 3
                                            radius: 10
                                            color: "transparent"
                                            border.color: "#5b3e73"
                                            border.width: 4
                                            opacity: root.motionEnabled ? animatedOpacity : 1.0
                                            z: 2
                                            Accessible.ignored: true

                                            SequentialAnimation on animatedOpacity {
                                                objectName: "inactivityHintAnimation"
                                                running: inactivityHint.visible && root.motionEnabled
                                                loops: Animation.Infinite
                                                NumberAnimation { to: 1.0; duration: 450 }
                                                NumberAnimation { to: 0.45; duration: 450 }
                                            }
                                        }

                                        MouseArea {
                                            id: pitchCellMouse
                                            objectName: "pitchCellMouse-%1-%2"
                                                .arg(parent.stepIndex).arg(parent.pitch)
                                            anchors.fill: parent
                                            // A single entry cell represents the whole timeline in
                                            // the Tab chain; arrows reach every other destination.
                                            activeFocusOnTab: parent.stepIndex === 0
                                                && parent.visualRow === 0
                                            KeyNavigation.tab: activeFocusOnTab
                                                ? (undoButton.enabled ? undoButton : eraserButton)
                                                : null
                                            KeyNavigation.backtab: activeFocusOnTab
                                                ? (addMeasureButton.enabled
                                                   ? addMeasureButton : removeMeasureButton)
                                                : null
                                            KeyNavigation.priority: KeyNavigation.BeforeItem
                                            onActiveFocusChanged: {
                                                if (activeFocus) {
                                                    root.timelineFocusStep = parent.stepIndex
                                                    root.timelineFocusLane = parent.visualRow
                                                    root.revealTimelineItem(this)
                                                }
                                            }
                                            cursorShape: root.toolPointerActive ? Qt.BlankCursor : Qt.ArrowCursor
                                            Accessible.role: Accessible.Button
                                            Accessible.name: root.eraseMode
                                                ? qsTr("Erase pitch %1 on beat %2")
                                                      .arg(parent.pitch + 1).arg(parent.stepIndex + 1)
                                                : app.selectedKind === "pitched"
                                                  ? qsTr("Place %1 on pitch %2, beat %3")
                                                        .arg(root.soundNames[app.selectedSound])
                                                        .arg(parent.pitch + 1).arg(parent.stepIndex + 1)
                                                  : qsTr("Pitch %1, beat %2")
                                                        .arg(parent.pitch + 1).arg(parent.stepIndex + 1)
                                            Accessible.onPressAction:
                                                root.activatePitchCell(parent.stepIndex, parent.pitch)
                                            onClicked: root.activatePitchCell(parent.stepIndex, parent.pitch)
                                            Keys.onSpacePressed:
                                                root.activatePitchCell(parent.stepIndex, parent.pitch)
                                            Keys.onReturnPressed:
                                                root.activatePitchCell(parent.stepIndex, parent.pitch)
                                            Keys.onPressed: event => root.handleTimelineKey(
                                                event, parent.stepIndex, parent.visualRow)
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                x: 0
                                y: root.pitchLaneHeight + (root.compactLayout ? 2 : 8)
                                width: parent.width
                                height: root.compactLayout ? 2 : 4
                                radius: 2
                                color: "#6e557d"
                                opacity: 0.45
                            }

                            Grid {
                                id: drumLane
                                objectName: "drumLane"
                                x: 0
                                y: root.drumLaneTop
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
                                        height: root.drumRowHeight
                                        radius: 10
                                        color: Qt.rgba(root.drumColors[drumRow].r,
                                                       root.drumColors[drumRow].g,
                                                       root.drumColors[drumRow].b, 0.15)
                                        border.color: drumCellMouse.activeFocus
                                            ? "#2b1a3d"
                                            : stepIndex % 4 === 0 ? "#9a6c82" : "#d8b8bf"
                                        border.width: drumCellMouse.activeFocus
                                            ? 4 : stepIndex % 4 === 0 ? 2 : 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: root.drumMarks[parent.drumRow]
                                            color: root.drumColors[parent.drumRow]
                                            font.pixelSize: 17
                                            opacity: 0.65
                                        }

                                        MouseArea {
                                            id: drumCellMouse
                                            objectName: "drumCellMouse-%1-%2"
                                                .arg(parent.stepIndex).arg(parent.drumRow)
                                            anchors.fill: parent
                                            activeFocusOnTab: false
                                            onActiveFocusChanged: {
                                                if (activeFocus) {
                                                    root.timelineFocusStep = parent.stepIndex
                                                    root.timelineFocusLane = 7 + parent.drumRow
                                                    root.revealTimelineItem(this)
                                                }
                                            }
                                            cursorShape: root.toolPointerActive ? Qt.BlankCursor : Qt.ArrowCursor
                                            Accessible.role: Accessible.Button
                                            Accessible.name: root.eraseMode
                                                ? qsTr("Erase %1 on beat %2")
                                                      .arg(root.drumNames[parent.drumRow])
                                                      .arg(parent.stepIndex + 1)
                                                : qsTr("%1 drum row, beat %2")
                                                      .arg(root.drumNames[parent.drumRow])
                                                      .arg(parent.stepIndex + 1)
                                            Accessible.onPressAction:
                                                root.activateDrumCell(parent.stepIndex, parent.drumRow)
                                            onClicked:
                                                root.activateDrumCell(parent.stepIndex, parent.drumRow)
                                            Keys.onSpacePressed:
                                                root.activateDrumCell(parent.stepIndex, parent.drumRow)
                                            Keys.onReturnPressed:
                                                root.activateDrumCell(parent.stepIndex, parent.drumRow)
                                            Keys.onPressed: event => root.handleTimelineKey(
                                                event, parent.stepIndex, 7 + parent.drumRow)
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
                                visible: app.compositionPlaying && currentStep >= 0
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
                                    property bool sounding: app.compositionPlaying && app.playbackStep === step
                                    property real popScale: 1.0
                                    objectName: "compositionToken-%1-%2".arg(step).arg(pitchRow)
                                    x: step * root.cellWidth + 12
                                    y: pitched
                                       ? (6 - pitchRow) * (root.rowHeight + root.rowGap)
                                           + (root.rowHeight - 42) / 2
                                       : root.drumLaneTop + 5
                                           + pitchRow * (root.drumRowHeight + root.rowGap)
                                    width: pitched ? 42 : 40
                                    height: pitched ? 42 : 34
                                    radius: pitched ? 21 : 10
                                    color: pitched ? root.soundColors[soundId]
                                                   : root.drumColors[soundId]
                                    border.color: sounding ? "#2b1a3d" : "#ffffff"
                                    border.width: sounding ? 6 : 3
                                    scale: root.compactLayout ? 1.0 : popScale
                                    z: 5

                                    Rectangle {
                                        objectName: parent.pitched
                                            ? "pitchedTokenFocusRing-%1-%2"
                                                  .arg(parent.step).arg(parent.pitchRow)
                                            : "percussionTokenFocusRing-%1-%2"
                                                  .arg(parent.step).arg(parent.pitchRow)
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        radius: Math.max(4, parent.radius - 2)
                                        visible: tokenMouse.activeFocus
                                        color: "transparent"
                                        border.color: "#21152f"
                                        border.width: 4
                                        z: 2
                                        Accessible.ignored: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.pitched ? root.soundMarks[parent.soundId]
                                                             : root.drumMarks[parent.soundId]
                                        color: "#21152f"
                                        font.pixelSize: parent.pitched ? 23 : 19
                                        font.bold: true
                                    }

                                    SequentialAnimation on popScale {
                                        running: root.placementAnimationsEnabled
                                        NumberAnimation { to: 1.18; duration: 90 }
                                        NumberAnimation { to: 1.0; duration: 150 }
                                    }

                                    MouseArea {
                                        id: tokenMouse
                                        objectName: parent.pitched
                                            ? "pitchedTokenMouse-%1-%2"
                                                  .arg(parent.step).arg(parent.pitchRow)
                                            : "percussionTokenMouse-%1-%2"
                                                  .arg(parent.step).arg(parent.pitchRow)
                                        anchors.centerIn: parent
                                        width: Math.max(44, parent.width)
                                        height: Math.max(44, parent.height)
                                        activeFocusOnTab: false
                                        onActiveFocusChanged: {
                                            if (activeFocus) {
                                                root.timelineFocusStep = parent.step
                                                root.timelineFocusLane = parent.pitched
                                                    ? 6 - parent.pitchRow : 7 + parent.pitchRow
                                                root.revealTimelineItem(this)
                                            }
                                        }
                                        cursorShape: root.toolPointerActive ? Qt.BlankCursor : Qt.ArrowCursor
                                        Accessible.role: Accessible.Button
                                        Accessible.name: parent.pitched
                                            ? qsTr("Placed %1 on pitch %2, beat %3")
                                                  .arg(root.soundNames[parent.soundId])
                                                  .arg(parent.pitchRow + 1).arg(parent.step + 1)
                                            : qsTr("Placed %1 on beat %2")
                                                  .arg(root.drumNames[parent.soundId])
                                                  .arg(parent.step + 1)
                                        Accessible.onPressAction: parent.pitched
                                            ? root.activatePitchCell(parent.step, parent.pitchRow)
                                            : root.activateDrumCell(parent.step, parent.pitchRow)
                                        onClicked: parent.pitched
                                            ? root.activatePitchCell(parent.step, parent.pitchRow)
                                            : root.activateDrumCell(parent.step, parent.pitchRow)
                                        Keys.onSpacePressed: parent.pitched
                                            ? root.activatePitchCell(parent.step, parent.pitchRow)
                                            : root.activateDrumCell(parent.step, parent.pitchRow)
                                        Keys.onReturnPressed: parent.pitched
                                            ? root.activatePitchCell(parent.step, parent.pitchRow)
                                            : root.activateDrumCell(parent.step, parent.pitchRow)
                                        Keys.onPressed: event => root.handleTimelineKey(
                                            event, parent.step,
                                            parent.pitched ? 6 - parent.pitchRow
                                                           : 7 + parent.pitchRow)
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
                                    cursorShape: root.toolPointerActive ? Qt.BlankCursor : Qt.ArrowCursor
                                }
                            }

                            Rectangle {
                                id: activeToolIndicator
                                objectName: "activeToolIndicator"
                                readonly property bool eraserTool: root.eraseMode
                                readonly property real destinationLeft:
                                    Math.floor(compositionHover.point.position.x
                                               / root.cellWidth) * root.cellWidth
                                readonly property real rightOfDestination:
                                    destinationLeft + root.cellWidth - root.rowGap + 8
                                width: 36
                                height: 36
                                x: {
                                    const viewportLeft = timeline.contentX
                                    const viewportRight = viewportLeft + timeline.width
                                    const preferred = rightOfDestination + width <= viewportRight
                                                      ? rightOfDestination
                                                      : destinationLeft - width - 8
                                    return Math.max(viewportLeft,
                                                    Math.min(viewportRight - width, preferred))
                                }
                                y: Math.max(timeline.contentY,
                                            Math.min(timeline.contentY + timeline.height - height,
                                                     compositionHover.point.position.y - height / 2))
                                visible: compositionHover.hovered && root.toolPointerActive
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

    Item {
        id: notificationTray
        objectName: "notificationTray"
        readonly property int visibleCount:
            (app.saveFailed && !app.loadFailed ? 1 : 0)
            + (app.audioFailed && !app.loadFailed ? 1 : 0)
            + (root.placementFeedbackVisible ? 1 : 0)
        x: 24
        y: Math.max(0, Math.min(20,
                               clearSongButton.mapToItem(null, 0, 0).y - 3 - height))
        width: root.width < 1004 ? 440 : 720
        height: notificationColumn.implicitHeight
        visible: visibleCount > 0
        z: 200

        ColumnLayout {
            id: notificationColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 3

            Rectangle {
                objectName: "saveFailureBanner"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(28, saveFailureLabel.implicitHeight + 8)
                visible: app.saveFailed && !app.loadFailed
                radius: 8
                color: "#fff0c2"
                border.color: "#d69f32"
                border.width: 2
                Accessible.name: app.saveFailureMessage
                Accessible.role: Accessible.AlertMessage
                onVisibleChanged: {
                    if (visible)
                        Accessible.announce(app.saveFailureMessage, Accessible.Polite)
                }

                Label {
                    id: saveFailureLabel
                    objectName: "saveFailureLabel"
                    anchors.centerIn: parent
                    width: parent.width - 16
                    text: app.saveFailureMessage
                    color: "#5c3b00"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Rectangle {
                objectName: "audioFailureBanner"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(28, audioFailureLabel.implicitHeight + 8)
                visible: app.audioFailed && !app.loadFailed
                radius: 8
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
                    id: audioFailureLabel
                    objectName: "audioFailureLabel"
                    anchors.centerIn: parent
                    width: parent.width - 16
                    text: app.audioFailureMessage
                    color: "#123e52"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Rectangle {
                objectName: "placementFeedback"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(28, placementFeedbackLabel.implicitHeight + 8)
                visible: root.placementFeedbackVisible
                radius: 8
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
                    id: placementFeedbackLabel
                    objectName: "placementFeedbackLabel"
                    anchors.centerIn: parent
                    width: parent.width - 16
                    text: root.placementFeedbackMessage
                    color: "#3a2948"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Popup {
        id: loadFailurePopup
        objectName: "loadFailurePopup"
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, root.width - 48)
        modal: true
        focus: true
        visible: app.loadFailed
        closePolicy: Popup.NoAutoClose
        padding: 28
        onAboutToShow: root.recoveryActionFailed = false
        onOpened: recoveryActionButton.forceActiveFocus(Qt.PopupFocusReason)

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

            Label {
                objectName: "recoveryActionFailure"
                Layout.fillWidth: true
                visible: root.recoveryActionFailed
                text: qsTr("We couldn't keep this song safe yet. Please try again.")
                color: "#7a3d00"
                font.pixelSize: 16
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Accessible.name: text
                Accessible.role: Accessible.AlertMessage
                onVisibleChanged: {
                    if (visible)
                        Accessible.announce(text, Accessible.Polite)
                }
            }

            Button {
                id: recoveryActionButton
                objectName: "recoveryActionButton"
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: 44
                Layout.minimumHeight: 44
                text: qsTr("Keep it safe and start a new song")
                onClicked: {
                    root.markInteraction()
                    root.recoveryActionFailed = false
                    if (!app.preserveFailedAutosaveAndStartNew())
                        root.recoveryActionFailed = true
                }
                Accessible.name: qsTr("Preserve the old song and start a new song")
            }
        }
    }
}
