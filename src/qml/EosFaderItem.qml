import QtQuick 2.5
import CustomElements 1.0
import "CustomBasics"
import "CustomControls"

StretchColumn {
    implicitWidth: -1
    leftMargin: 5*dp
    rightMargin: 5*dp

    property int index
    property bool feedbackEnabled: true
    property bool ledState: false

    onLedStateChanged: {
        controller.midiMapping().sendFeedback(fireButton.mappingID, ledState ? 1.0 : 0.0)
    }

    function is_gm(s) {
        var patt = /^GM( \(BO\))?/;
        return patt.test(s)
    }

    function is_bo(s) {
        var patt = /^GM \(BO\)/;
        return patt.test(s)
    }

    property bool grandmaster: is_gm(block.faderLabels[index])

    Text {
        height: 40*dp
        text: block.faderLabels[index]
        horizontalAlignment: Text.AlignHCenter
        fontSizeMode: Text.Fit
        color: (text[0] === "S") ? "lightgreen" : "white"
        onTextChanged: {
            if (is_bo(text))
                controller.midiMapping().sendFeedback(fireButton.mappingID, 1.0)
            else
                controller.midiMapping().sendFeedback(fireButton.mappingID, 0.0)
        }
    }
    PushButton {
        implicitHeight: 0  // do not stretch
        height: 40*dp
        text: index + 1
        onTouchDown: {
            if (touch.modifiers & Qt.ShiftModifier) {
                block.sendUnloadEvent(index)
            } else {
                block.sendLoadEvent(index, true)
            }
        }
        onTouchUp: {
            if (!(touch.modifiers & Qt.ShiftModifier)) {
                block.sendLoadEvent(index, false)
            }
        }
        showLed: false
        mappingID: block.getUid() + "load" + modelData
        onExternalInputChanged: {
            if (externalInput > 0.) {
                block.sendLoadEvent(index, true)
            } else {
                block.sendLoadEvent(index, false)
            }
        }
    }
    Slider {
        implicitHeight: -1
        value: block.faderLevels[index]
        onValueChanged: {
            if (value !== block.faderLevels[index]) {
                block.setFaderLevelFromGui(index, value)
            }
        }
        onExternalValueChanged: {
            block.setFaderLevelFromExt(index, externalValue)
        }
        mappingID: block.getUid() + "fader" + modelData
        feedbackEnabled: parent.feedbackEnabled
    }
    PushButton {
        id: stopButton
        implicitHeight: 0  // do not stretch
        height: 40*dp
        onActiveChanged: {
            if (pageChangeMode.active) {
                if (active)
                    block.setPageFromGui(index + 1)
            }
            else if (!grandmaster) {
                block.sendStopEvent(index, active)
            }
        }

        showLed: false
        mappingID: block.getUid() + "stop" + modelData
        Image {
            width: 12*dp
            height: 12*dp
            anchors.centerIn: parent
            anchors.verticalCenterOffset: parent.pressed ? -2*dp : -4*dp
            source: "qrc:/images/fader_stop_icon.png"
        }
    }
    PushButton {
        id: fireButton
        implicitHeight: 0  // do not stretch
        height: 40*dp
        toggle: grandmaster
        extEnabled: (grandmaster && stopButton.active) || !grandmaster
        active: is_bo()
        onActiveChanged: {
            if (grandmaster) {
                block.sendBlEvent(1)
            }
            else {
                block.sendFireEvent(index, active)
            }
        }
        showLed: false
        mappingID: block.getUid() + "go" + modelData
        Image {
            width: 15*dp
            height: 15*dp
            anchors.centerIn: parent
            anchors.verticalCenterOffset: parent.pressed ? -2*dp : -4*dp
            source: "qrc:/images/fader_go_icon.png"
        }
    }
}
