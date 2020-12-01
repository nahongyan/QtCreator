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

#include "processhandle.h"

namespace Utils {

/*!
    \class Utils::ProcessHandle
    \brief ProcessHandle类是描述流程的辅助类。封装正在运行的进程的参数，本地(PID)或远程(待定)完成，地址，端口，等等。


*/

// 这与QProcess中的情况是一样的，即Qt不关心进程#0。
const qint64 InvalidPid = 0;

ProcessHandle::ProcessHandle()
    : m_pid(InvalidPid)
{
}

ProcessHandle::ProcessHandle(qint64 pid)
    : m_pid(pid)
{
}

bool ProcessHandle::isValid() const
{
    return m_pid != InvalidPid;
}

void ProcessHandle::setPid(qint64 pid)
{
    m_pid = pid;
}

qint64 ProcessHandle::pid() const
{
    return m_pid;
}

bool ProcessHandle::equals(const ProcessHandle &rhs) const
{
    return m_pid == rhs.m_pid;
}

#ifndef Q_OS_OSX
bool ProcessHandle::activate()
{
    return false;
}
#endif

} // Utils
