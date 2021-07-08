import QtQuick 2.5
import CustomElements 1.0
import "../../CustomBasics"
import "../../CustomControls"
import "../../."  // to import EosFaderItem

BlockBase {
    id: root
    width: (block.attr("numFaders").val)*90*dp
    height: 400*dp
    settingsComponent: settings

    Timer {
        interval: 200
        running: true
        repeat: true
        property bool state: true
        onTriggered: {
            for (var i = 0; i < block.attr("numFaders").val; i++) {
                if (!block.faderSync[i]) {
                    if (faderRepeater.itemAt(i).ledState !== state)
                        faderRepeater.itemAt(i).ledState = state
                }
                else {
                    if (faderRepeater.itemAt(i).ledState !== false)
                        faderRepeater.itemAt(i).ledState = false
                }
            }
            state = !state
        }
    }

    StretchColumn {
        anchors.fill: parent

        StretchRow {
            implicitHeight: -1

            Repeater {
                id: faderRepeater
                model: block.attr("numFaders").val

                EosFaderItem {
                    index: modelData
                    feedbackEnabled: block.attr("feedbackEnabled").val
                }
            }
        }

        DragArea {
            text: "Fader Bank"

            StretchRow {
                width: 180*dp
                height: 30*dp
                anchors.right: parent.right
                anchors.rightMargin: 15*dp

                ButtonBottomLine {
                    id: pageChangeMode
                    width: 80*dp
                    text: "Page:"
                    mappingID: block.getUid() + "pageChange"
                    visible: block.attr("masterPair").val === false
                }
                ButtonBottomLine {
                    width: 30*dp
                    text: "-"
                    onPress: block.sendPageMinusEvent()
                    mappingID: block.getUid() + "pageMinus"
                    visible: block.attr("masterPair").val === false
                }
                NumericInput {
                    width: 40*dp
                    minimumValue: 1
                    maximumValue: 100
                    value: block.page
                    onValueChanged: {
                        if (value !== block.page) {
                            block.page = value
                        }
                    }
                    visible: block.attr("masterPair").val === false
                }
                ButtonBottomLine {
                    width: 30*dp
                    text: "+"
                    onPress: block.sendPagePlusEvent()
                    mappingID: block.getUid() + "pagePlus"
                    visible: block.attr("masterPair").val === false
                }
            }

            OutputNode {
                node: block.node("outputNode")
            }
        }

    }  // end main Column

    Component {
        id: settings
        StretchColumn {
            leftMargin: 15*dp
            rightMargin: 15*dp
            defaultSize: 30*dp

            BlockRow {
                Text {
                    text: "Master fader pair"
                    width: parent.width - 30*dp
                }
                AttributeCheckbox {
                    id: masterPair
                    width: 30*dp
                    attr: block.attr("masterPair")
                }
            }

            BlockRow {
                Text {
                    text: "Catch external faders:"
                    width: parent.width - 30*dp
                }
                AttributeCheckbox {
                    width: 30*dp
                    attr: block.attr("catchFaders")
                }
            }
            BlockRow {
                StretchText {
                    text: "Faders count:"
                }
                AttributeNumericInput {
                    width:  55*dp
                    implicitWidth: 0
                    attr: block.attr("numFaders")
                }
                visible: block.attr("masterPair").val === false
            }
            BlockRow {
                Text {
                    text: "Feedback enabled:"
                    width: parent.width - 30*dp
                }
                AttributeCheckbox {
                    width: 30*dp
                    attr: block.attr("feedbackEnabled")
                }
            }
        }
    }  // end Settings Component
}
