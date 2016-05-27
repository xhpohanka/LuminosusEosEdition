import QtQuick 2.5
import CustomElements 1.0
import "../CustomBasics"
import "../CustomControls"

BlockBase {
	id: root
	width: 90*dp
	height: 60*dp
	pressed: dragarea.pressed

	StretchColumn {
		anchors.fill: parent

		NumericInput {
			minimumValue: 0
			maximumValue: 999
			decimals: 2
			suffix: "s"
			value: block.decay
			onValueChanged: {
				if (value !== block.decay) {
					block.decay = value
				}
			}
		}

		DragArea {
			id: dragarea
			guiBlock: root
			text: "Decay"
			OutputNode {
				anchors.left: parent.left
				objectName: "outputNode"
			}
			InputNode {
				anchors.right: parent.right
				objectName: "inputNode"
			}
		}
	}
}
