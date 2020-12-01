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

#include <QString>
#include <QHash>
#include <QVector>

QT_BEGIN_NAMESPACE
class QRegularExpression;
QT_END_NAMESPACE

namespace ExtensionSystem {

namespace Internal {

class OptionsParser;
class PluginSpecPrivate;
class PluginManagerPrivate;

} // Internal

class IPlugin;
class PluginView;

struct EXTENSIONSYSTEM_EXPORT PluginDependency
{
    enum Type {
            Required, // 必须有此依赖
            Optional, // 此依赖不是必须的,在设计插件时需要注意，插件在无此依赖时应该能够正常加载,比如，不能使用被依赖插件的 API 等
            Test
    };
/*!
一个插件可能建立在其它插件的基础之上。如果插件 A 必须在插件 B 加载成功之后才能够加载，
那么我们就说，插件 A 依赖于插件 B，插件 B 是插件 A 的被依赖插件。按照这一的加载模式，最终 Qt Creator 会得到一棵插件树。
PluginDependency定义了有关被依赖插件的信息，包括被依赖插件的名字以及版本号等。
我们使用PluginDependency定义所需要的依赖，Qt Creator 则根据我们的定义，利用 Qt 的反射机制，
通过名字和版本号获取到插件对应的状态，从而获知被依赖插件是否加载之类的信息。值得注意的是，Qt Creator 在匹配版本号时，
并不会直接按照这里给出的version值完全匹配，而是按照一定的算法，选择一段区间内兼容的版本。
这样做的目的是，有些插件升级了版本号之后，另外的插件可以按照版本号兼容，不需要一同升级
*/
    PluginDependency() : type(Required) {}
    QString name; // 被依赖插件名字
    QString version; // 被依赖插件版本号
    Type type; // 依赖类型
    bool operator==(const PluginDependency &other) const;
    QString toString() const;
};

/*!
qHash()函数是一个全局函数，用于计算PluginDependency类的散列值。
*/
uint qHash(const ExtensionSystem::PluginDependency &value);

/*!
PluginArgumentDescription是一个简单的数据类，用于描述插件参数。
Qt Creator 的插件可以在启动时提供额外的参数，类似main()函数参数的作用。
*/
struct EXTENSIONSYSTEM_EXPORT PluginArgumentDescription
{
    QString name;//名称
    QString parameter;//参数
    QString description;//描述
};
/*!
最主要的PluginSpec类的定义。首先，是一个State枚举，用于指示插件加载时的状态。
当插件加载失败时，我们可以根据插件的状态来判断是哪个环节出了问题。这些状态的含义如下:
Invalid	起始点：任何信息都没有读取，甚至连插件元数据都没有读到
Read	成功读取插件元数据，并且该元数据是合法的；此时，插件的相关信息已经可用
Resolved	插件描述文件中给出的各个依赖已经被成功找到，这些依赖可以通过dependencySpecs()函数获取
Loaded	插件的库已经加载，插件实例成功创建；此时插件实例可以通过plugin()函数获取
Initialized	调用插件实例的IPlugin::initialize()函数，并且该函数返回成功
Running	插件的依赖成功初始化，并且调用了extensionsInitialized()函数；此时，加载过程完毕
Stopped	插件已经停止，插件的IPlugin::aboutToShutdown()函数被调用
Deleted	插件实例被删除销毁
*/
class EXTENSIONSYSTEM_EXPORT PluginSpec
{
public:

    enum State { Invalid, Read, Resolved, Loaded, Initialized, Running, Stopped, Deleted};

    ~PluginSpec();

    // 插件名字。当状态达到 PluginSpec::Read 时才可用。
    QString name() const;
    // 插件版本。当状态达到 PluginSpec::Read 时才可用。
    QString version() const;
    // 插件版次。当状态达到 PluginSpec::Read 时才可用。
    QString revision() const;
    //返回插件兼容版本. 当状态达到 PluginSpec::Read 时才可用
    QString compatVersion() const;
    // 插件提供者。当状态达到 PluginSpec::Read 时才可用。
    QString vendor() const;
    // 插件版权。当状态达到 PluginSpec::Read 时才可用。
    QString copyright() const;
    // 返回插件许可。当状态达到 PluginSpec::Read 时才可用。
    QString license() const;
    // 插件描述。当状态达到 PluginSpec::Read 时才可用。
    QString description() const;
    // 插件主页 URL。当状态达到 PluginSpec::Read 时才可用。
    QString url() const;
    // 插件类别，用于在界面分组显示插件信息。如果插件不属于任何类别，直接返回空字符串。
    QString category() const;
    // 插件兼容的平台版本的正则表达式。如果兼容所有平台，则返回空。
    QRegularExpression platformSpecification() const;
    // 对于宿主平台是否可用。该函数用使用 platformSpecification() 的返回值对平台名字进行匹配。
    bool isAvailableForHostPlatform() const;
    // 是否必须。
    bool isRequired() const;
    // 是否实验性质的插件。
    bool isExperimental() const;
    // 默认启用。实验性质的插件可能会被禁用。
    bool isEnabledByDefault() const;
    // 因配置信息启动。
    bool isEnabledBySettings() const;
    // 是否在启动时已经加载。
    bool isEffectivelyEnabled() const;
    // 因为用户取消或者因其依赖项被取消而导致该插件无法加载时，返回 true。
    bool isEnabledIndirectly() const;
    // 是否通过命令行参数 -load 加载。
    bool isForceEnabled() const;
    // 是否通过命令行参数 -noload 禁用。
    bool isForceDisabled() const;
    // 插件依赖列表。当状态达到 PluginSpec::Read 时才可用。
    QVector<PluginDependency> dependencies() const;
    QJsonObject metaData() const;
    using PluginArgumentDescriptions = QVector<PluginArgumentDescription>;
    // 插件处理的命令行参数描述符列表。
    PluginArgumentDescriptions argumentDescriptions() const;
    // 该 PluginSpec 实例对应的插件 XML 描述文件所在目录的绝对位置。
    QString location() const;
    // 该 PluginSpec 实例对应的插件 XML 描述文件的绝对位置（包含文件名）。
    QString filePath() const;
    // 插件命令行参数。启动时设置。
    QStringList arguments() const;
    // 设置插件命令行参数为 arguments。
    void setArguments(const QStringList &arguments);
    // 将 argument 添加到插件的命令行参数。
    void addArgument(const QString &argument);
    // 当一个依赖需要插件名为 pluginName、版本为 version 时，返回该插件是否满足。
    bool provides(const QString &pluginName, const QString &version) const;
    //插件的依赖。当状态达到 PluginSpec::Resolved 时才可用
    QHash<PluginDependency, PluginSpec *> dependencySpecs() const;
    //是否依赖 plugins 集合中的任一插件
    bool requiresAny(const QSet<PluginSpec *> &plugins) const;
    // PluginSpec 实例对应的 IPlugin 实例。当状态达到 PluginSpec::Loaded 时才可用。
    IPlugin *plugin() const;

    // 当前状态。
    State state() const;
    // 是否发生错误。
    bool hasError() const;
    // 错误信息。
    QString errorString() const;
    //设置开启通过设置
    void setEnabledBySettings(bool value);

    static PluginSpec *read(const QString &filePath);

/*!
PluginSpec的构造函数是私有的。这意味着我们不能创建其实例。
这个类显然不是单例，并且明显没有提供static的工厂函数，那么，我们如何创建其实例呢？
答案就是，我们不能：PluginSpec的实例只能通过 Qt Creator 自身创建，而能够创建的类，
就是这里定义的友元类。这里其实使用了 C++ 语言特性，即友元类可以访问到私有函数。
我们将Internal::PluginManagerPrivate设置为PluginSpec的友元，就可以通过这个类调用PluginSpec私有的构造函数，
从而创建其实例。这一技巧依赖于 C++ 语言特性，不能推广到其它语言，不过如果你使用的正是 C++，
那么不妨尝试使用这一技巧，实现一种只能通过系统本身才能实例化的类。
*/
private:
    PluginSpec();

    Internal::PluginSpecPrivate *d;
    friend class PluginView;
    friend class Internal::OptionsParser;
    friend class Internal::PluginManagerPrivate;
    friend class Internal::PluginSpecPrivate;
};

} // namespace ExtensionSystem
