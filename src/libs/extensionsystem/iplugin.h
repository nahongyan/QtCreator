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

#pragma once

#include "extensionsystem_global.h"

#include <QObject>

namespace ExtensionSystem {

namespace Internal {
    class IPluginPrivate;
    class PluginSpecPrivate;
}

class PluginManager;
class PluginSpec;

class EXTENSIONSYSTEM_EXPORT IPlugin : public QObject
{
    Q_OBJECT

public:
    enum ShutdownFlag {
        SynchronousShutdown,
        AsynchronousShutdown
    };

    IPlugin();
    ~IPlugin() override;

/*!
这是一个纯虚函数，因此每个插件都必须实现这个函数。该函数会在插件加载完成，并且创建了插件对应的IPlugin对象之后调用。
该函数返回值是bool类型，当插件初始化成功，返回true；否则，需要在errorString参数中设置人可读的错误信息。
注意，该函数的调用顺序是依赖树从根到叶子，因此，所有依赖该插件的插件的initialize()函数，都会在该插件自己的initialize()函数之后被调用。
如果插件需要共享一些对象，就应该将这些共享对象放在这个函数中。
*/
    virtual bool initialize(const QStringList &arguments, QString *errorString) = 0;

/*!
该函数会在IPlugin::initialize()调用完毕、并且自己所依赖插件的IPlugin::initialize()和
IPlugin::extensionsInitialized()调用完毕之后被调用。在这个函数中，插件可以假设自己所依赖的插件已经成功加载并且正常运行。
当运行到这一阶段时，插件所依赖的其它插件都已经初始化完毕。
这暗示着，该插件所依赖的各个插件提供的可被共享的对象都已经创建完毕（这是在IPlugin::initialize()函数中完成的），可以正常使用了。
如果插件的库文件加载失败，或者插件初始化失败，所有依赖该插件的插件都会失败
*/
    virtual void extensionsInitialized() {}

/*!
这是除了IPlugin::initialize()和IPlugin::extensionsInitialized()两个函数之外，另外一个与启动有关的函数。
该函数会在所有插件的IPlugin::extensionsInitialized()函数调用完成、同时所依赖插件的IPlugin::delayedInitialize()函数也调用完成之后才被调用，
也就是延迟初始化。插件的IPlugin::delayedInitialize()函数会在程序运行之后才被调用，并且距离程序启动有几个毫秒的间隔。
为避免不必要的延迟，插件对该函数的实现应该尽快返回。该函数的意义在于，有些插件可能需要进行一些重要的启动工作；
这些工作虽然不必在启动时直接完成，但也应该在程序启动之后的较短时间内完成。
该函数默认返回false，即不需要延迟初始化。如果插件有这类需求，就可以重写这个函数。
*/
    virtual bool delayedInitialize() { return false; }

/*!
aboutToShutdown()函数会以插件初始化的相反顺序调用。该函数应该用于与其它插件断开连接、隐藏所有 UI、优化关闭操作。
如果插件需要延迟真正的关闭，例如，需要等待外部进程执行完毕，以便自己完全关闭，则应该返回IPlugin::AsynchronousShutdown。
这么做的话会进入主事件循环，等待所有返回了IPlugin::AsynchronousShutdown的插件都发出了asynchronousShutdownFinished()信号之后，
再执行相关操作。该函数默认实现是不作任何操作，直接返回IPlugin::SynchronousShutdown，即不等待其它插件关闭。
*/
    virtual ShutdownFlag aboutToShutdown() { return SynchronousShutdown; }
    virtual QObject *remoteCommand(const QStringList & /* 选项 */,
                                   const QString & /* 工作目录 */,
                                   const QStringList & /* 参数 */) { return nullptr; }
    virtual QVector<QObject *> createTestObjects() const;


    PluginSpec *pluginSpec() const;

signals:
    void asynchronousShutdownFinished();

private:
//IPlugin将对象池操作委托给PluginManager类
    Internal::IPluginPrivate *d;

    friend class Internal::PluginSpecPrivate;
};

} // namespace ExtensionSystem
