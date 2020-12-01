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

#include "networkaccessmanager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QNetworkReply>

#ifdef Q_OS_UNIX
#include <sys/utsname.h>
#endif

/*!
   \class Utils::NetworkAccessManager
   \inmodule QtCreator

    \brief NetworkAccessManager类提供一个可供使用的网络访问管理器与\ QC。通用初始化，\QC用户代理。最好，
NetworkAccessManager::instance()返回的实例应该用于main线程。构造函数仅供多线程使用。
 */

namespace Utils {

static NetworkAccessManager *namInstance = nullptr;

void cleanupNetworkAccessManager()
{
    delete namInstance;
    namInstance = nullptr;
}

/*!
返回一个网络访问管理器实例，该实例应用于主服务器线程。
*/
NetworkAccessManager *NetworkAccessManager::instance()
{
    if (!namInstance) {
        namInstance = new NetworkAccessManager;
        qAddPostRoutine(cleanupNetworkAccessManager);
    }
    return namInstance;
}

/*!
    构造一个带有父组件的网络访问管理器实例。
*/
NetworkAccessManager::NetworkAccessManager(QObject *parent)
    : QNetworkAccessManager(parent)
{

}

/*!
    为网络访问管理器创建请求，以对输出数据执行操作op。

*/
QNetworkReply* NetworkAccessManager::createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
{
    QString agentStr = QString::fromLatin1("%1/%2 (QNetworkAccessManager %3; %4; %5; %6 bit)")
                    .arg(QCoreApplication::applicationName(),
                         QCoreApplication::applicationVersion(),
                         QLatin1String(qVersion()),
                         QSysInfo::prettyProductName(),
                         QLocale::system().name())
                    .arg(QSysInfo::WordSize);
    QNetworkRequest req(request);
    req.setRawHeader("User-Agent", agentStr.toLatin1());
    return QNetworkAccessManager::createRequest(op, req, outgoingData);
}


} // namespace utils
