/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.0

SplitView {
    width: 600
    height: 400

    Rectangle {
        id: column
        width: 200
        Layout.minimumWidth: 100
        Layout.maximumWidth: 300
        color: "lightsteelblue"
    }

    SplitView {
        orientation: Qt.Vertical
        Layout.fillWidth: true

        Rectangle {
            id: row1
            height: 200
            color: "lightblue"
            Layout.minimumHeight: 1
        }

        Rectangle {
            id: row2
            color: "lightgray"
        }
    }
}


