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

#include "statuslabel.h"

#include <QTimer>

/*!
    \class Utils::StatusLabel

    \brief StatusLabel类在一段时间内显示带有超时的消息。
*/

namespace Utils {

StatusLabel::StatusLabel(QWidget *parent) : QLabel(parent)
{
    //手动大小让我们缩小到最小文本宽度以下,我们想要进入[假]状态栏。

    setMinimumSize(QSize(30, 10));
}

void StatusLabel::stopTimer()
{
    if (m_timer && m_timer->isActive())
        m_timer->stop();
}

void StatusLabel::showStatusMessage(const QString &message, int timeoutMS)
{
    setText(message);
    if (timeoutMS > 0) {
        if (!m_timer) {
            m_timer = new QTimer(this);
            m_timer->setSingleShot(true);
            connect(m_timer, &QTimer::timeout, this, &StatusLabel::slotTimeout);
        }
        m_timer->start(timeoutMS);
    } else {
        m_lastPermanentStatusMessage = message;
        stopTimer();
    }
}

void StatusLabel::slotTimeout()
{
    setText(m_lastPermanentStatusMessage);
}

void StatusLabel::clearStatusMessage()
{
    stopTimer();
    m_lastPermanentStatusMessage.clear();
    clear();
}

} // namespace Utils
