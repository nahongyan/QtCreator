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

#include "iplugin.h"
#include "iplugin_p.h"
#include "pluginmanager.h"
#include "pluginspec.h"

/*!
    \class ExtensionSystem::IPlugin
    \inheaderfile extensionsystem/iplugin.h
    \inmodule QtCreator
    \ingroup mainclasses

摘要: IPlugin类是一个抽象基类，每个插件必须实现一次。除了实际的插件库之外，插件还需要提供元数据，
这样插件管理器就可以找到插件，解决它的依赖关系，并加载它。有关更多信息，请参见{Plugin元数据}。
插件必须提供IPlugin类的一个实现，它位于与元数据中给出的name属性相匹配的库中。
IPlugin实现必须导出并让Qt的插件系统知道,使用 Q_PLUGIN_METADATA宏
IID设置为"org.qt . project. qt . qtcreatorplugin "。
*/

/*!
    \enum IPlugin::ShutdownFlag
无论插件是同步关闭还是同步关闭，这个枚举类型都将保持异步。
SynchronousShutdown 插件是同步关闭的。
AsynchronousShutdown 插件需要异步执行关闭前的操作。
*/

/*!
    \fn bool ExtensionSystem::IPlugin::initialize(const QStringList &arguments, QString *errorString)
在加载插件和IPlugin实例后调用已创建。调用依赖于此插件的插件的初始化函数在这个插件的初始化函数被调用后一个参数。
插件应该在这里初始化它们的内部状态函数。返回初始化是否成功。如果没有，一个错误字符串应该设置为用户可读的消息，描述原因。
    \sa extensionsInitialized()
    \sa delayedInitialize()
*/

/*!
    \fn void ExtensionSystem::IPlugin::extensionsInitialized()
在initialize()函数被调用后调用，在initialize()和extensionsInitialized()之后依赖于此插件的插件的函数已经被调用。
在这个函数中，插件可以假定依赖于的插件这个插件已经完全启动并运行了。在全局对象池中查找已提供的对象通过弱依赖的插件。

    \sa initialize()
    \sa delayedInitialize()
*/

/*!
    \fn bool ExtensionSystem::IPlugin::delayedInitialize()
在所有插件的extensionsInitialized()函数被调用之后调用，在依赖于此插件的插件的delayedInitialize()函数被调用之后调用。
插件的 delayedInitialize()函数是在应用程序已经运行之后调用的，在应用程序启动之前会有几毫秒的延迟，
并且在单独的 delayedInitialize()函数调用之间调用。为了避免不必要的延迟，如果一个插件实际实现了它，
它应该从函数返回true，以表明下一个插件的delayedInitialize()调用应该延迟几毫秒，以给输入和绘制事件一个被处理的机会。
如果插件需要做一些不需要在启动时直接进行的琐碎设置，但仍然需要在启动后短时间内完成，那么可以使用这个函数。
这可以大大减少插件或应用程序的启动时间。
    \sa initialize()
    \sa extensionsInitialized()
*/

/*!
    \fn ExtensionSystem::IPlugin::ShutdownFlag ExtensionSystem::IPlugin::aboutToShutdown()
在关机过程中调用，其顺序与插件被删除之前的初始化顺序相同。这个函数应该用于断开与其他插件的连接，隐藏所有UI，以及优化一般的关机。
如果一个插件需要延迟真正的关闭一段时间，例如，如果它需要等待外部进程完成一个干净的关闭，
插件可以从这个函数返回IPlugin::AsynchronousShutdown。
这将使主事件循环在aboutToShutdown()序列完成后继续运行，直到所有请求异步关闭的插件都发送了异步shutdownfinished()信号。
这个函数的默认实现不执行任何操作，并返回IPlugin::SynchronousShutdown。如果插件需要在关闭之前执行异步操作，
返回IPlugin::AsynchronousShutdown。

    \sa asynchronousShutdownFinished()
*/

/*!
    \fn QObject *ExtensionSystem::IPlugin::remoteCommand(const QStringList &options,
                                           const QString &workingDirectory,
                                           const QStringList &arguments)
当另一个QC实例正在运行时，带 -client参数执行QC时，插件的这个函数将在运行的实例中调用。
 workingDirectory参数指定调用进程的工作目录。例如，如果您在一个目录中执行qtcreator -client文件。
调用进程的工作目录被传递给运行的实例和文件。将转换为从该目录开始的绝对路径。特定于插件的参数在a选项中传递，
而其余的参数在a参数中传递。如果使用 -block，则返回一个QObject，该QObject阻塞命令，直到该命令被销毁。
*/

/*!
    \fn void ExtensionSystem::IPlugin::asynchronousShutdownFinished()
发送后，插件实现异步关机准备继续执行关闭序列。

    \sa aboutToShutdown()
*/

using namespace ExtensionSystem;

/*!
    \internal
*/
IPlugin::IPlugin()
    : d(new Internal::IPluginPrivate())
{
}

/*!
    \internal
*/
IPlugin::~IPlugin()
{
    delete d;
    d = nullptr;
}

/*!
返回要传递给 QTest::qExec()的对象。
如果用户启动QC，将调用此函数-test PluginName或-test all。返回对象的所有权转移给调用者。
*/
QVector<QObject *> IPlugin::createTestObjects() const
{
    return {};
}

/*!
返回与此插件相对应的PluginSpec。在构造函数中不可用。
*/
PluginSpec *IPlugin::pluginSpec() const
{
    return d->pluginSpec;
}
