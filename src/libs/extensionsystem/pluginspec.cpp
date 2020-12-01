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

#include "pluginspec.h"

#include "pluginspec_p.h"
#include "iplugin.h"
#include "iplugin_p.h"
#include "pluginmanager.h"

#include <utils/algorithm.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPluginLoader>

/*!
    \class ExtensionSystem::PluginDependency
    \inheaderfile extensionsystem/pluginspec.h
    \inmodule QtCreator

    \brief PluginDependency类包含名称和必需的兼容性插件依赖的版本号。
这反映了插件元数据中依赖对象的数据。名称和版本用于解析依赖项。也就是说,
一个具有给定名称和搜索plugin 兼容性版本<=依赖版本<= plugin版本。
请参阅ExtensionSystem::IPlugin以获得更多关于插件依赖关系的信息版本匹配。
*/

/*!
    \variable ExtensionSystem::PluginDependency::name
    插件的字符串标识符
*/

/*!
    \variable ExtensionSystem::PluginDependency::version
    插件必须匹配的版本字符串来填充此依赖项。
*/

/*!
    \variable ExtensionSystem::PluginDependency::type
    定义依赖项是必需的还是可选的。
    \sa ExtensionSystem::PluginDependency::Type
*/

/*!
    \enum ExtensionSystem::PluginDependency::Type
    依赖性是必需的还是可选的。
    \value Required
           需要有依赖性。
    \value Optional
依赖性是不必要的。你需要确定插件可以在不安装此依赖项的情况下加载例如，您可以不链接到依赖项的库。
    \value Test
           为了运行插件的测试，需要强制加载依赖项。
*/

/*!
    \class ExtensionSystem::PluginSpec
    \inheaderfile extensionsystem/pluginspec.h
    \inmodule QtCreator

    \brief PluginSpec类包含插件嵌入元数据的信息以及关于插件当前状态的信息。
插件规范中也包含了插件的更多信息通过它的加载过程(参见PluginSpec::State)。
如果错误发生，插件规范是寻找的错误地方的细节。
*/

/*!
    \enum ExtensionSystem::PluginSpec::State
State enum表示插件所经历的状态它正在被加载。在出现错误的情况下，状态给出了出错的提示。

    \value  Invalid
            起点:甚至插件元数据都没有读取。
    \value  Read
插件元数据已被成功读取这些信息可以通过PluginSpec获得
    \value  Resolved
在描述文件中给出的依赖关系已经成功找到，并通过dependencySpecs()函数可用
    \value  Loaded
加载插件库并创建插件实例(可以通过插件())。
    \value  Initialized
已经调用了插件实例的IPlugin::initialize()函数并返回一个success值。
    \value  Running
插件的依赖项被成功初始化已调用extensionsInitialized。加载过程完成了。
    \value Stopped
            该插件已经被关闭，即该插件的IPlugin::aboutToShutdown()函数已经被调用。
    \value Deleted
            插件实例已被删除。
*/

/*!
    \class ExtensionSystem::PluginArgumentDescription
    \inheaderfile extensionsystem/pluginspec.h
    \inmodule QtCreator

    \brief PluginArgumentDescriptions类 的描述列表插件处理的命令行参数。
    \sa PluginSpec::argumentDescriptions()
*/

using namespace ExtensionSystem;
using namespace ExtensionSystem::Internal;

/*!
qHash()函数是一个全局函数，用于计算PluginDependency类的散列值。
该函数的作用是允许PluginDependency类作为QHash这样的集合类的键。按照QHash文档要求，
QHash的键必须在其类型所在命名空间中同时提供operator==()以及qHash()函数。
有关具体细节，可以参考QHash的相关文档。在这里，这个函数的实现相当简单：只是计算了name属性的散列值
*/
uint ExtensionSystem::qHash(const PluginDependency &value)
{
    return qHash(value.name);
}

/*!
    \internal
*/
bool PluginDependency::operator==(const PluginDependency &other) const
{
    return name == other.name && version == other.version && type == other.type;
}

static QString typeString(PluginDependency::Type type)
{
    switch (type) {
    case PluginDependency::Optional:
        return QString(", optional");
    case PluginDependency::Test:
        return QString(", test");
    case PluginDependency::Required:
    default:
        return QString();
    }
}

/*!
    \internal
*/
QString PluginDependency::toString() const
{
    return name + " (" + version + typeString(type) + ")";
}

/*!
    \internal
*/
PluginSpec::PluginSpec()
    : d(new PluginSpecPrivate(this))
{
}

/*!
    \internal
*/
PluginSpec::~PluginSpec()
{
    delete d;
    d = nullptr;
}

/*!
返回插件名称。这在PluginSpec::Read状态是有效时才可用。
*/
QString PluginSpec::name() const
{
    return d->name;
}

/*!
返回插件版本。当状态达到 PluginSpec::Read 时才可用。

*/
QString PluginSpec::version() const
{
    return d->version;
}

/*!
    返回插件兼容版本. 当状态达到 PluginSpec::Read 时才可用
*/
QString PluginSpec::compatVersion() const
{
    return d->compatVersion;
}

/*!
    返回插件供应商 当状态达到 PluginSpec::Read 时才可用
*/
QString PluginSpec::vendor() const
{
    return d->vendor;
}

/*!
    返回插件的版权。状态达到 PluginSpec::Read 时才可用

*/
QString PluginSpec::copyright() const
{
    return d->copyright;
}

/*!
    返回插件许可. 状态达到 PluginSpec::Read 时才可用
*/
QString PluginSpec::license() const
{
    return d->license;
}

/*!
    返回插件描述 状态达到 PluginSpec::Read 时才可用
*/
QString PluginSpec::description() const
{
    return d->description;
}

/*!
    返回插件URL，在那里你可以找到更多关于插件的信息。状态达到 PluginSpec::Read 时才可用
*/
QString PluginSpec::url() const
{
    return d->url;
}

/*!
返回插件所属的类别。类别用于在UI中将插件组在一起。如果插件不属于某个类别，则返回一个空字符串。
*/
QString PluginSpec::category() const
{
    return d->category;
}

QString PluginSpec::revision() const
{
    const QJsonValue revision = metaData().value("Revision");
    if (revision.isString())
        return revision.toString();
    return QString();
}

/*!
返回与插件工作平台匹配的QRegularExpression。空模式意味着所有平台。

    \since 3.0
*/

QRegularExpression PluginSpec::platformSpecification() const
{
    return d->platformSpecification;
}

/*!
    返回插件是否在主机平台上工作。
*/
bool PluginSpec::isAvailableForHostPlatform() const
{
    return d->platformSpecification.pattern().isEmpty()
            || d->platformSpecification.match(PluginManager::platformName()).hasMatch();
}

/*!
    返回是否需要插件。
*/
bool PluginSpec::isRequired() const
{
    return d->required;
}

/*!
    返回插件是否设置了它的实验标志
*/
bool PluginSpec::isExperimental() const
{
    return d->experimental;
}

/*!
返回插件是否默认启用。一个插件被禁用可能是因为这个插件是试验性的，或者因为
默认情况下，安装设置将其定义为禁用。
*/
bool PluginSpec::isEnabledByDefault() const
{
    return d->enabledByDefault;
}

/*!
返回插件是否应该在启动时加载，考虑默认的启用状态和用户的设置。
这个函数可能会返回 false，即使加载了插件作为另一个启用的插件的要求。

    \sa isEffectivelyEnabled()
*/
bool PluginSpec::isEnabledBySettings() const
{
    return d->enabledBySettings;
}

/*!
    返回插件是否在启动时加载。
    \sa isEnabledBySettings()
*/
bool PluginSpec::isEffectivelyEnabled() const
{
    if (!isAvailableForHostPlatform())
        return false;
    if (isForceEnabled() || isEnabledIndirectly())
        return true;
    if (isForceDisabled())
        return false;
    return isEnabledBySettings();
}

/*!
如果由于用户取消选择插件或其依赖关系而没有加载，则返回 true。
*/
bool PluginSpec::isEnabledIndirectly() const
{
    return d->enabledIndirectly;
}

/*!
属性上的 -load选项是否启用插件命令行。

*/
bool PluginSpec::isForceEnabled() const
{
    return d->forceEnabled;
}

/*!
属性上的-noload选项返回插件是否被禁用命令行。

*/
bool PluginSpec::isForceDisabled() const
{
    return d->forceDisabled;
}

/*!
    插件依赖关系。这在达到PluginSpec::Read状态后有效。
*/
QVector<PluginDependency> PluginSpec::dependencies() const
{
    return d->dependencies;
}

/*!
    返回插件元数据。
*/
QJsonObject PluginSpec::metaData() const
{
    return d->metaData;
}

/*!
    返回插件处理的命令行参数的描述列表。
*/

PluginSpec::PluginArgumentDescriptions PluginSpec::argumentDescriptions() const
{
    return d->argumentDescriptions;
}

/*!
    返回包含插件的目录的绝对路径
*/
QString PluginSpec::location() const
{
    return d->location;
}

/*!
    返回插件的绝对路径。
*/
QString PluginSpec::filePath() const
{
    return d->filePath;
}

/*!
    返回特定于插件的命令行参数。设置在启动。
*/

QStringList PluginSpec::arguments() const
{
    return d->arguments;
}

/*!
    将插件特有的命令行参数设置为参数。
*/

void PluginSpec::setArguments(const QStringList &arguments)
{
    d->arguments = arguments;
}

/*!
   为插件特定的命令行参数添加一个参数
*/

void PluginSpec::addArgument(const QString &argument)
{
    d->arguments.push_back(argument);
}


/*!
返回插件当前的状态。请参阅PluginSpec::State enum的描述以获得详细信息。
*/
PluginSpec::State PluginSpec::state() const
{
    return d->state;
}

/*!
  返回在读取或启动插件时是否发生错误。
*/
bool PluginSpec::hasError() const
{
    return d->hasError;
}

/*!
返回详细的(可能是多行的)错误描述。

*/
QString PluginSpec::errorString() const
{
    return d->errorString;
}

/*!
返回这个插件是否可以用来填充给定的依赖项一个插件名和一个版本。
        \sa PluginSpec::dependencies()
*/
bool PluginSpec::provides(const QString &pluginName, const QString &version) const
{
    return d->provides(pluginName, version);
}

/*!
返回相应的IPlugin实例，如果插件库有已成功加载。也就是说，PluginSpec::Loaded 是达到了。
*/
IPlugin *PluginSpec::plugin() const
{
    return d->plugin;
}

/*!
返回已解析到现有插件规格的依赖项列表。如果达到了PluginSpec::Resolved，则有效。
    \sa PluginSpec::dependencies()
*/
QHash<PluginDependency, PluginSpec *> PluginSpec::dependencySpecs() const
{
    return d->dependencySpecs;
}

/*!
返回插件是否需要指定的任何插件plugins。

*/
bool PluginSpec::requiresAny(const QSet<PluginSpec *> &plugins) const
{
    return Utils::anyOf(d->dependencySpecs.keys(), [this, &plugins](const PluginDependency &dep) {
        return dep.type == PluginDependency::Required
               && plugins.contains(d->dependencySpecs.value(dep));
    });
}

/*!
    设置插件是否在启动时加载为一个值。

    \sa isEnabledBySettings()
*/
void PluginSpec::setEnabledBySettings(bool value)
{
    d->setEnabledBySettings(value);
}

PluginSpec *PluginSpec::read(const QString &filePath)
{
    auto spec = new PluginSpec;
    if (!spec->d->read(filePath)) { // 不是Qt Creator插件
        delete spec;
        return nullptr;
    }
    return spec;
}

//==========PluginSpecPrivate==================

namespace {
    const char PLUGIN_METADATA[] = "MetaData";
    const char PLUGIN_NAME[] = "Name";
    const char PLUGIN_VERSION[] = "Version";
    const char PLUGIN_COMPATVERSION[] = "CompatVersion";
    const char PLUGIN_REQUIRED[] = "Required";
    const char PLUGIN_EXPERIMENTAL[] = "Experimental";
    const char PLUGIN_DISABLED_BY_DEFAULT[] = "DisabledByDefault";
    const char VENDOR[] = "Vendor";
    const char COPYRIGHT[] = "Copyright";
    const char LICENSE[] = "License";
    const char DESCRIPTION[] = "Description";
    const char URL[] = "Url";
    const char CATEGORY[] = "Category";
    const char PLATFORM[] = "Platform";
    const char DEPENDENCIES[] = "Dependencies";
    const char DEPENDENCY_NAME[] = "Name";
    const char DEPENDENCY_VERSION[] = "Version";
    const char DEPENDENCY_TYPE[] = "Type";
    const char DEPENDENCY_TYPE_SOFT[] = "optional";
    const char DEPENDENCY_TYPE_HARD[] = "required";
    const char DEPENDENCY_TYPE_TEST[] = "test";
    const char ARGUMENTS[] = "Arguments";
    const char ARGUMENT_NAME[] = "Name";
    const char ARGUMENT_PARAMETER[] = "Parameter";
    const char ARGUMENT_DESCRIPTION[] = "Description";
}
/*!
    \internal
*/
PluginSpecPrivate::PluginSpecPrivate(PluginSpec *spec)
    : q(spec)
{
    if (Utils::HostOsInfo::isMacHost())
        loader.setLoadHints(QLibrary::ExportExternalSymbolsHint);
}

/*!
    \internal
    如果文件不代表Qt Creator插件，则返回false。
*/
bool PluginSpecPrivate::read(const QString &fileName)
{
    qCDebug(pluginLog) << "\nReading meta data of" << fileName;
    name
        = version
        = compatVersion
        = vendor
        = copyright
        = license
        = description
        = url
        = category
        = location
        = QString();
    state = PluginSpec::Invalid;
    hasError = false;
    errorString.clear();
    dependencies.clear();
    metaData = QJsonObject();
    QFileInfo fileInfo(fileName);
    location = fileInfo.absolutePath();
    filePath = fileInfo.absoluteFilePath();
    loader.setFileName(filePath);
    if (loader.fileName().isEmpty()) {
        qCDebug(pluginLog) << "Cannot open file";
        return false;
    }

    if (!readMetaData(loader.metaData()))
        return false;

    state = PluginSpec::Read;
    return true;
}

void PluginSpecPrivate::setEnabledBySettings(bool value)
{
    enabledBySettings = value;
}

void PluginSpecPrivate::setEnabledByDefault(bool value)
{
    enabledByDefault = value;
}

void PluginSpecPrivate::setForceEnabled(bool value)
{
    forceEnabled = value;
    if (value)
        forceDisabled = false;
}

void PluginSpecPrivate::setForceDisabled(bool value)
{
    if (value)
        forceEnabled = false;
    forceDisabled = value;
}

/*!
    \internal
*/
bool PluginSpecPrivate::reportError(const QString &err)
{
    errorString = err;
    hasError = true;
    return true;
}

static inline QString msgValueMissing(const char *key)
{
    return QCoreApplication::translate("PluginSpec", "\"%1\" is missing").arg(QLatin1String(key));
}

static inline QString msgValueIsNotAString(const char *key)
{
    return QCoreApplication::translate("PluginSpec", "Value for key \"%1\" is not a string")
            .arg(QLatin1String(key));
}

static inline QString msgValueIsNotABool(const char *key)
{
    return QCoreApplication::translate("PluginSpec", "Value for key \"%1\" is not a bool")
            .arg(QLatin1String(key));
}

static inline QString msgValueIsNotAObjectArray(const char *key)
{
    return QCoreApplication::translate("PluginSpec", "Value for key \"%1\" is not an array of objects")
            .arg(QLatin1String(key));
}

static inline QString msgValueIsNotAMultilineString(const char *key)
{
    return QCoreApplication::translate("PluginSpec", "Value for key \"%1\" is not a string and not an array of strings")
            .arg(QLatin1String(key));
}

static inline QString msgInvalidFormat(const char *key, const QString &content)
{
    return QCoreApplication::translate("PluginSpec", "Value \"%2\" for key \"%1\" has invalid format")
            .arg(QLatin1String(key), content);
}

/*!
    \internal
*/
bool PluginSpecPrivate::readMetaData(const QJsonObject &pluginMetaData)
{
    qCDebug(pluginLog) << "MetaData:" << QJsonDocument(pluginMetaData).toJson();
    QJsonValue value;
    value = pluginMetaData.value(QLatin1String("IID"));
    if (!value.isString()) {
        qCDebug(pluginLog) << "Not a plugin (no string IID found)";
        return false;
    }
    if (value.toString() != PluginManager::pluginIID()) {
        qCDebug(pluginLog) << "Plugin ignored (IID does not match)";
        return false;
    }

    value = pluginMetaData.value(QLatin1String(PLUGIN_METADATA));
    if (!value.isObject())
        return reportError(tr("Plugin meta data not found"));
    metaData = value.toObject();

    value = metaData.value(QLatin1String(PLUGIN_NAME));
    if (value.isUndefined())
        return reportError(msgValueMissing(PLUGIN_NAME));
    if (!value.isString())
        return reportError(msgValueIsNotAString(PLUGIN_NAME));
    name = value.toString();

    value = metaData.value(QLatin1String(PLUGIN_VERSION));
    if (value.isUndefined())
        return reportError(msgValueMissing(PLUGIN_VERSION));
    if (!value.isString())
        return reportError(msgValueIsNotAString(PLUGIN_VERSION));
    version = value.toString();
    if (!isValidVersion(version))
        return reportError(msgInvalidFormat(PLUGIN_VERSION, version));

    value = metaData.value(QLatin1String(PLUGIN_COMPATVERSION));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(PLUGIN_COMPATVERSION));
    compatVersion = value.toString(version);
    if (!value.isUndefined() && !isValidVersion(compatVersion))
        return reportError(msgInvalidFormat(PLUGIN_COMPATVERSION, compatVersion));

    value = metaData.value(QLatin1String(PLUGIN_REQUIRED));
    if (!value.isUndefined() && !value.isBool())
        return reportError(msgValueIsNotABool(PLUGIN_REQUIRED));
    required = value.toBool(false);
    qCDebug(pluginLog) << "required =" << required;

    value = metaData.value(QLatin1String(PLUGIN_EXPERIMENTAL));
    if (!value.isUndefined() && !value.isBool())
        return reportError(msgValueIsNotABool(PLUGIN_EXPERIMENTAL));
    experimental = value.toBool(false);
    qCDebug(pluginLog) << "experimental =" << experimental;

    value = metaData.value(QLatin1String(PLUGIN_DISABLED_BY_DEFAULT));
    if (!value.isUndefined() && !value.isBool())
        return reportError(msgValueIsNotABool(PLUGIN_DISABLED_BY_DEFAULT));
    enabledByDefault = !value.toBool(false);
    qCDebug(pluginLog) << "enabledByDefault =" << enabledByDefault;

    if (experimental)
        enabledByDefault = false;
    enabledBySettings = enabledByDefault;

    value = metaData.value(QLatin1String(VENDOR));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(VENDOR));
    vendor = value.toString();

    value = metaData.value(QLatin1String(COPYRIGHT));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(COPYRIGHT));
    copyright = value.toString();

    value = metaData.value(QLatin1String(DESCRIPTION));
    if (!value.isUndefined() && !Utils::readMultiLineString(value, &description))
        return reportError(msgValueIsNotAString(DESCRIPTION));

    value = metaData.value(QLatin1String(URL));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(URL));
    url = value.toString();

    value = metaData.value(QLatin1String(CATEGORY));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(CATEGORY));
    category = value.toString();

    value = metaData.value(QLatin1String(LICENSE));
    if (!value.isUndefined() && !Utils::readMultiLineString(value, &license))
        return reportError(msgValueIsNotAMultilineString(LICENSE));

    value = metaData.value(QLatin1String(PLATFORM));
    if (!value.isUndefined() && !value.isString())
        return reportError(msgValueIsNotAString(PLATFORM));
    const QString platformSpec = value.toString().trimmed();
    if (!platformSpec.isEmpty()) {
        platformSpecification.setPattern(platformSpec);
        if (!platformSpecification.isValid())
            return reportError(tr("Invalid platform specification \"%1\": %2")
                               .arg(platformSpec, platformSpecification.errorString()));
    }

    value = metaData.value(QLatin1String(DEPENDENCIES));
    if (!value.isUndefined() && !value.isArray())
        return reportError(msgValueIsNotAObjectArray(DEPENDENCIES));
    if (!value.isUndefined()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &v : array) {
            if (!v.isObject())
                return reportError(msgValueIsNotAObjectArray(DEPENDENCIES));
            QJsonObject dependencyObject = v.toObject();
            PluginDependency dep;
            value = dependencyObject.value(QLatin1String(DEPENDENCY_NAME));
            if (value.isUndefined())
                return reportError(tr("Dependency: %1").arg(msgValueMissing(DEPENDENCY_NAME)));
            if (!value.isString())
                return reportError(tr("Dependency: %1").arg(msgValueIsNotAString(DEPENDENCY_NAME)));
            dep.name = value.toString();
            value = dependencyObject.value(QLatin1String(DEPENDENCY_VERSION));
            if (!value.isUndefined() && !value.isString())
                return reportError(tr("Dependency: %1").arg(msgValueIsNotAString(DEPENDENCY_VERSION)));
            dep.version = value.toString();
            if (!isValidVersion(dep.version))
                return reportError(tr("Dependency: %1").arg(msgInvalidFormat(DEPENDENCY_VERSION,
                                                                             dep.version)));
            dep.type = PluginDependency::Required;
            value = dependencyObject.value(QLatin1String(DEPENDENCY_TYPE));
            if (!value.isUndefined() && !value.isString())
                return reportError(tr("Dependency: %1").arg(msgValueIsNotAString(DEPENDENCY_TYPE)));
            if (!value.isUndefined()) {
                const QString typeValue = value.toString();
                if (typeValue.toLower() == QLatin1String(DEPENDENCY_TYPE_HARD)) {
                    dep.type = PluginDependency::Required;
                } else if (typeValue.toLower() == QLatin1String(DEPENDENCY_TYPE_SOFT)) {
                    dep.type = PluginDependency::Optional;
                } else if (typeValue.toLower() == QLatin1String(DEPENDENCY_TYPE_TEST)) {
                    dep.type = PluginDependency::Test;
                } else {
                    return reportError(tr("Dependency: \"%1\" must be \"%2\" or \"%3\" (is \"%4\").")
                                       .arg(QLatin1String(DEPENDENCY_TYPE),
                                            QLatin1String(DEPENDENCY_TYPE_HARD),
                                            QLatin1String(DEPENDENCY_TYPE_SOFT),
                                            typeValue));
                }
            }
            dependencies.append(dep);
        }
    }

    value = metaData.value(QLatin1String(ARGUMENTS));
    if (!value.isUndefined() && !value.isArray())
        return reportError(msgValueIsNotAObjectArray(ARGUMENTS));
    if (!value.isUndefined()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &v : array) {
            if (!v.isObject())
                return reportError(msgValueIsNotAObjectArray(ARGUMENTS));
            QJsonObject argumentObject = v.toObject();
            PluginArgumentDescription arg;
            value = argumentObject.value(QLatin1String(ARGUMENT_NAME));
            if (value.isUndefined())
                return reportError(tr("Argument: %1").arg(msgValueMissing(ARGUMENT_NAME)));
            if (!value.isString())
                return reportError(tr("Argument: %1").arg(msgValueIsNotAString(ARGUMENT_NAME)));
            arg.name = value.toString();
            if (arg.name.isEmpty())
                return reportError(tr("Argument: \"%1\" is empty").arg(QLatin1String(ARGUMENT_NAME)));
            value = argumentObject.value(QLatin1String(ARGUMENT_DESCRIPTION));
            if (!value.isUndefined() && !value.isString())
                return reportError(tr("Argument: %1").arg(msgValueIsNotAString(ARGUMENT_DESCRIPTION)));
            arg.description = value.toString();
            value = argumentObject.value(QLatin1String(ARGUMENT_PARAMETER));
            if (!value.isUndefined() && !value.isString())
                return reportError(tr("Argument: %1").arg(msgValueIsNotAString(ARGUMENT_PARAMETER)));
            arg.parameter = value.toString();
            argumentDescriptions.append(arg);
            qCDebug(pluginLog) << "Argument:" << arg.name << "Parameter:" << arg.parameter
                               << "Description:" << arg.description;
        }
    }

    return true;
}

/*!
    \internal
*/
bool PluginSpecPrivate::provides(const QString &pluginName, const QString &pluginVersion) const
{
    if (QString::compare(pluginName, name, Qt::CaseInsensitive) != 0)
        return false;
    return (versionCompare(version, pluginVersion) >= 0) && (versionCompare(compatVersion, pluginVersion) <= 0);
}

/*!
    \internal
*/
const QRegularExpression &PluginSpecPrivate::versionRegExp()
{
    static const QRegularExpression reg("^([0-9]+)(?:[.]([0-9]+))?(?:[.]([0-9]+))?(?:_([0-9]+))?$");
    return reg;
}
/*!
    \internal
*/
bool PluginSpecPrivate::isValidVersion(const QString &version)
{
    return versionRegExp().match(version).hasMatch();
}

/*!
    \internal
*/
int PluginSpecPrivate::versionCompare(const QString &version1, const QString &version2)
{
    const QRegularExpressionMatch match1 = versionRegExp().match(version1);
    const QRegularExpressionMatch match2 = versionRegExp().match(version2);
    if (!match1.hasMatch())
        return 0;
    if (!match2.hasMatch())
        return 0;
    for (int i = 0; i < 4; ++i) {
        const int number1 = match1.captured(i + 1).toInt();
        const int number2 = match2.captured(i + 1).toInt();
        if (number1 < number2)
            return -1;
        if (number1 > number2)
            return 1;
    }
    return 0;
}

/*!
    \internal
*/
bool PluginSpecPrivate::resolveDependencies(const QVector<PluginSpec *> &specs)
{
    if (hasError)
        return false;
    if (state == PluginSpec::Resolved)
        state = PluginSpec::Read; // 返回，这样我们就重新解决了依赖关系
    if (state != PluginSpec::Read) {
        errorString = QCoreApplication::translate("PluginSpec", "Resolving dependencies failed because state != Read");
        hasError = true;
        return false;
    }
    QHash<PluginDependency, PluginSpec *> resolvedDependencies;
    for (const PluginDependency &dependency : qAsConst(dependencies)) {
        PluginSpec * const found = Utils::findOrDefault(specs, [&dependency](PluginSpec *spec) {
            return spec->provides(dependency.name, dependency.version);
        });
        if (!found) {
            if (dependency.type == PluginDependency::Required) {
                hasError = true;
                if (!errorString.isEmpty())
                    errorString.append(QLatin1Char('\n'));
                errorString.append(QCoreApplication::translate("PluginSpec", "Could not resolve dependency '%1(%2)'")
                    .arg(dependency.name).arg(dependency.version));
            }
            continue;
        }
        resolvedDependencies.insert(dependency, found);
    }
    if (hasError)
        return false;

    dependencySpecs = resolvedDependencies;

    state = PluginSpec::Resolved;

    return true;
}

// 返回它实际上间接启用的插件
QVector<PluginSpec *> PluginSpecPrivate::enableDependenciesIndirectly(bool enableTestDependencies)
{
    if (!q->isEffectivelyEnabled()) //插件没有启用，没有做什么
        return {};
    QVector<PluginSpec *> enabled;
    for (auto it = dependencySpecs.cbegin(), end = dependencySpecs.cend(); it != end; ++it) {
        if (it.key().type != PluginDependency::Required
                && (!enableTestDependencies || it.key().type != PluginDependency::Test))
            continue;
        PluginSpec *dependencySpec = it.value();
        if (!dependencySpec->isEffectivelyEnabled()) {
            dependencySpec->d->enabledIndirectly = true;
            enabled << dependencySpec;
        }
    }
    return enabled;
}

/*!
    \internal
*/
bool PluginSpecPrivate::loadLibrary()
{
    if (hasError)
        return false;
    if (state != PluginSpec::Resolved) {
        if (state == PluginSpec::Loaded)
            return true;
        errorString = QCoreApplication::translate("PluginSpec", "Loading the library failed because state != Resolved");
        hasError = true;
        return false;
    }
    if (!loader.load()) {
        hasError = true;
        errorString = QDir::toNativeSeparators(filePath)
            + QString::fromLatin1(": ") + loader.errorString();
        return false;
    }
    auto *pluginObject = qobject_cast<IPlugin*>(loader.instance());
    if (!pluginObject) {
        hasError = true;
        errorString = QCoreApplication::translate("PluginSpec", "Plugin is not valid (does not derive from IPlugin)");
        loader.unload();
        return false;
    }
    state = PluginSpec::Loaded;
    plugin = pluginObject;
    plugin->d->pluginSpec = q;
    return true;
}

/*!
    \internal
*/
bool PluginSpecPrivate::initializePlugin()
{
    if (hasError)
        return false;
    if (state != PluginSpec::Loaded) {
        if (state == PluginSpec::Initialized)
            return true;
        errorString = QCoreApplication::translate("PluginSpec", "Initializing the plugin failed because state != Loaded");
        hasError = true;
        return false;
    }
    if (!plugin) {
        errorString = QCoreApplication::translate("PluginSpec", "Internal error: have no plugin instance to initialize");
        hasError = true;
        return false;
    }
    QString err;
    if (!plugin->initialize(arguments, &err)) {
        errorString = QCoreApplication::translate("PluginSpec", "Plugin initialization failed: %1").arg(err);
        hasError = true;
        return false;
    }
    state = PluginSpec::Initialized;
    return true;
}

/*!
    \internal
*/
bool PluginSpecPrivate::initializeExtensions()
{
    if (hasError)
        return false;
    if (state != PluginSpec::Initialized) {
        if (state == PluginSpec::Running)
            return true;
        errorString = QCoreApplication::translate("PluginSpec", "Cannot perform extensionsInitialized because state != Initialized");
        hasError = true;
        return false;
    }
    if (!plugin) {
        errorString = QCoreApplication::translate("PluginSpec", "Internal error: have no plugin instance to perform extensionsInitialized");
        hasError = true;
        return false;
    }
    plugin->extensionsInitialized();
    state = PluginSpec::Running;
    return true;
}

/*!
    \internal
*/
bool PluginSpecPrivate::delayedInitialize()
{
    if (hasError)
        return false;
    if (state != PluginSpec::Running)
        return false;
    if (!plugin) {
        errorString = QCoreApplication::translate("PluginSpec", "Internal error: have no plugin instance to perform delayedInitialize");
        hasError = true;
        return false;
    }
    return plugin->delayedInitialize();
}

/*!
    \internal
*/
IPlugin::ShutdownFlag PluginSpecPrivate::stop()
{
    if (!plugin)
        return IPlugin::SynchronousShutdown;
    state = PluginSpec::Stopped;
    return plugin->aboutToShutdown();
}

/*!
    \internal
*/
void PluginSpecPrivate::kill()
{
    if (!plugin)
        return;
    delete plugin;
    plugin = nullptr;
    state = PluginSpec::Deleted;
}
