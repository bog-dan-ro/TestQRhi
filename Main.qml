import QtQuick
import TestQRhi

Window {
    width: rhiItem.width + 10
    height: rhiItem.height + 10
    visible: true
    title: qsTr("Test QRhi")

    RhiItem {
        id: rhiItem
        anchors.centerIn: parent
    }
}
