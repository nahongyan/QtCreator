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

#include "pluginmanager.h"
#include "pluginmanager_p.h"
#include "pluginspec.h"
#include "pluginspec_p.h"
#include "optionsparser.h"
#include "iplugin.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QLibrary>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QMetaProperty>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <QWriteLocker>

#include <utils/algorithm.h>
#include <utils/benchmarker.h>
#include <utils/executeondestruction.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/mimetypes/mimedatabase.h>
#include <utils/qtcassert.h>
#include <utils/synchronousprocess.h>

#ifdef WITH_TESTS
#include <utils/hostosinfo.h>
#include <QTest>
#endif

#include <functional>
#include <memory>

Q_LOGGING_CATEGORY(pluginLog, "qtc.extensionsystem", QtWarningMsg)

const char C_IGNORED_PLUGINS[] = "Plugins/Ignored";
const char C_FORCEENABLED_PLUGINS[] = "Plugins/ForceEnabled";
const int DELAYED_INITIALIZE_INTERVAL = 20; // ms

enum { debugLeaks = 0 };

/*!
    \namespace ExtensionSystem
    \inmodule QtCreator
ExtensionSystem提供了属于核心插件系统。
基本的扩展系统包含插件管理器及其支持类，
以及必须由插件提供程序实现的IPlugin接口。
*/

/*!
    \namespace ExtensionSystem::Internal
    \internal
*/

/*!
    \class ExtensionSystem::PluginManager
    \inheaderfile extensionsystem/pluginmanager.h
    \inmodule QtCreator
    \ingroup mainclasses

PluginManager类实现了核心插件系统
管理插件、插件的生命周期和插件的注册对象。
插件管理器用于以下任务:
    \list
    \li Manage plugins and their state
    \li Manipulate a \e {common object pool}
    \endlist

插件必须从IPlugin类派生并具有IID:“org.qt-project.Qt.QtCreatorPlugin”。
插件管理器用于设置要搜索的文件系统目录列表
检索这些插件的状态信息，并加载它们。
通常，应用程序创建一个PluginManager实例并启动装载。
    \code
        //'插件'和子目录将被搜索插件
        PluginManager::setPluginPaths(QStringList("plugins"));
        PluginManager::loadPlugins(); // 试着加载所有的插件
    \endcode
此外，可以直接访问插件元数据、实例、
和状态。

第一节 对象池
插件(和所有人)可以向位于插件管理器中的公共池添加对象。池中的对象必须从QObject派生，没有其他先决条件。
可以通过getObject()和getObjectByName()函数从对象池中检索对象。
每当对象池的状态发生变化时，插件管理器就会发出相应的信号。
对象池的一个常见用法是插件(或应用程序)为其他插件提供一个扩展点，这是一个类或接口，可以被实现并添加到对象池中。
提供扩展点的插件在对象池中查找类或接口的实现。
    \code
        // 插件A提供了一个“MimeTypeHandler”扩展点
        // 在插件B:
        MyMimeTypeHandler *handler = new MyMimeTypeHandler();
        PluginManager::instance()->addObject(handler);
        // 在插件A:
        MimeTypeHandler *mimeHandler =PluginManager::getObject<MimeTypeHandler>();
    \endcode

Invoker类模板提供了使用软扩展点的语法糖，这些点可能由池中的对象提供，也可能不提供。
这种方法既不需要将用户插件链接到提供程序插件上，也不需要一个公共的共享头文件。
公开的接口由对象池中的提供者对象的可调用函数隐式地提供。
invoke()函数模板封装了ExtensionSystem::Invoker构造，用于未检查调用成功的常见情况。

    \code
        // 在“provide”插件A中:
        namespace PluginA {
        class SomeProvider : public QObject
        {
            Q_OBJECT

        public:
            Q_INVOKABLE QString doit(const QString &msg, int n) {
            {
                qDebug() << "我正在做" << msg;
                return QString::number(n);
            }
        };
        } // namespace PluginA


        // 在“user”插件B中:
        int someFuntionUsingPluginA()
        {
            using namespace ExtensionSystem;

            QObject *target = PluginManager::getObjectByClassName("PluginA::SomeProvider");

            if (target) {
                // 一些随机参数。.
                QString msg = "REALLY.";

                // 纯函数调用，没有返回值
                invoke<void>(target, "doit", msg, 2);

                // 没有返回值的普通函数。
                qDebug() << "Result: " << invoke<QString>(target, "doit", msg, 21);

                // 记录函数调用成功与否并返回值。
                Invoker<QString> in1(target, "doit", msg, 21);
                qDebug() << "Success: (expected)" << in1.wasSuccessful();

                // 尝试调用一个不存在的函数
                Invoker<QString> in2(target, "doitWrong", msg, 22);
                qDebug() << "Success (not expected):" << in2.wasSuccessful();

            } else {

                // 我们必须处理插件A的缺席。
            }
        };
    \endcode

传递给invoke()调用的参数的类型是从参数本身推导出来的，并且必须匹配?的类型
被调用函数的参数exactly。没有转换，甚至整数提升是适用的，因此要调用一个函数，使用long参数，使用long(43)或此类。
对象池操作函数是线程安全的
*/

/*!
    \fn template <typename T> *ExtensionSystem::PluginManager::getObject()

从对象池中检索给定类型的对象。此函数使用 qobject_cast来确定对象的类型。
如果中有多个给定类型的对象对象池，此函数将任意选择其中之一。

    \sa addObject()
*/

/*!
    \fn template <typename T, typename Predicate> *ExtensionSystem::PluginManager::getObject(Predicate predicate)

从匹配的对象池中检索给定类型的对象谓词。此函数使用 qobject_cast来确定对象的类型。
谓词必须是一个函数，取T *并返回一个bool。如果有多个对象与类型和谓词匹配，这个函数会任意选择其中一个。
    \sa addObject()
*/


using namespace Utils;

//提供了属于核心插件系统的类
namespace ExtensionSystem {

using namespace Internal;

static Internal::PluginManagerPrivate *d = nullptr;
static PluginManager *m_instance = nullptr;

/*!
    获取唯一的插件管理器实例。
*/
PluginManager *PluginManager::instance()
{
    return m_instance;
}

/*!
   创建插件管理器。应该只做一次申请。
*/
PluginManager::PluginManager()
{
    m_instance = this;
    d = new PluginManagerPrivate(this);
}

/*!
    \internal
*/
PluginManager::~PluginManager()
{
    delete d;
    d = nullptr;
}

/*!
将对象一个obj添加到对象池中，以便可以检索它再次从池的类型。
插件管理器不做任何内存管理。添加对象须从池中删除，并由负责该对象的人手动删除。
发出 objectadd()信号。
    \sa PluginManager::removeObject()
    \sa PluginManager::getObject()
    \sa PluginManager::getObjectByName()
*/
void PluginManager::addObject(QObject *obj)
{
    d->addObject(obj);
}

/*!
发出 aboutToRemoveObject()信号并删除对象obj自对象池。
*/
void PluginManager::removeObject(QObject *obj)
{
    d->removeObject(obj);
}

/*!
检索池中未筛选的所有对象的列表。
通常，客户端不需要调用此函数。

    \sa PluginManager::getObject()
*/
QVector<QObject *> PluginManager::allObjects()
{
    return d->allObjects;
}

/*!
    \internal
*/
QReadWriteLock *PluginManager::listLock()
{
    return &d->m_lock;
}

/*!
尝试加载之前找到的所有插件设置插件搜索路径。
插件的插件规格可用于检索有关单个插件的错误和状态信息。

    \sa setPluginPaths()
    \sa plugins()
*/
void PluginManager::loadPlugins()
{
    d->loadPlugins();
}

/*!
如果任何插件有错误，即使它是启用的，返回真。最有用的是在loadPlugins()之后调用。
*/
bool PluginManager::hasError()
{
    return Utils::anyOf(plugins(), [](PluginSpec *spec) {
        // 只有在启用插件时才显示错误
        return spec->hasError() && spec->isEffectivelyEnabled();
    });
}

const QStringList PluginManager::allErrors()
{
    return Utils::transform<QStringList>(Utils::filtered(plugins(), [](const PluginSpec *spec) {
        return spec->hasError() && spec->isEffectivelyEnabled();
    }), [](const PluginSpec *spec) {
        return spec->name().append(": ").append(spec->errorString());
    });
}

/*!
    返回所有需要加载规范的插件。递归依赖关系。
 */
const QSet<PluginSpec *> PluginManager::pluginsRequiringPlugin(PluginSpec *spec)
{
    QSet<PluginSpec *> dependingPlugins({spec});
    //递归地添加依赖于以下插件的插件这取决于规范
    for (PluginSpec *spec : d->loadQueue()) {
        if (spec->requiresAny(dependingPlugins))
            dependingPlugins.insert(spec);
    }
    dependingPlugins.remove(spec);
    return dependingPlugins;
}

/*!
    返回规范要求加载的所有插件。递归依赖关系
 */
const QSet<PluginSpec *> PluginManager::pluginsRequiredByPlugin(PluginSpec *spec)
{
    QSet<PluginSpec *> recursiveDependencies;
    recursiveDependencies.insert(spec);
    std::queue<PluginSpec *> queue;
    queue.push(spec);
    while (!queue.empty()) {
        PluginSpec *checkSpec = queue.front();
        queue.pop();
        const QHash<PluginDependency, PluginSpec *> deps = checkSpec->dependencySpecs();
        for (auto depIt = deps.cbegin(), end = deps.cend(); depIt != end; ++depIt) {
            if (depIt.key().type != PluginDependency::Required)
                continue;
            PluginSpec *depSpec = depIt.value();
            if (!recursiveDependencies.contains(depSpec)) {
                recursiveDependencies.insert(depSpec);
                queue.push(depSpec);
            }
        }
    }
    recursiveDependencies.remove(spec);
    return recursiveDependencies;
}

/*!
    关闭并删除所有插件。
*/
void PluginManager::shutdown()
{
    d->shutdown();
}

static QString filled(const QString &s, int min)
{
    return s + QString(qMax(0, min - s.size()), ' ');
}

QString PluginManager::systemInformation() const
{
    QString result;
    CommandLine qtDiag(HostOsInfo::withExecutableSuffix(
                QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qtdiag"));
    SynchronousProcess qtdiagProc;
    const SynchronousProcessResponse response = qtdiagProc.runBlocking(qtDiag);
    if (response.result == SynchronousProcessResponse::Finished)
        result += response.allOutput() + "\n";
    result += "Plugin information:\n\n";
    auto longestSpec = std::max_element(d->pluginSpecs.cbegin(), d->pluginSpecs.cend(),
                                        [](const PluginSpec *left, const PluginSpec *right) {
                                            return left->name().size() < right->name().size();
                                        });
    int size = (*longestSpec)->name().size();
    for (const PluginSpec *spec : plugins()) {
        result += QLatin1String(spec->isEffectivelyEnabled() ? "+ " : "  ") + filled(spec->name(), size) +
                  " " + spec->version() + "\n";
    }
    return result;
}

/*!
    路径列表是插件管理器搜索插件的路径。

    \sa setPluginPaths()
*/
QStringList PluginManager::pluginPaths()
{
    return d->pluginPaths;
}

/*!
设置插件路径。所有指定的路径及其子目录在树中搜索插件。
    \sa pluginPaths()
    \sa loadPlugins()
*/
void PluginManager::setPluginPaths(const QStringList &paths)
{
    d->setPluginPaths(paths);
}

/*!
    有效的插件必须有IID。
    \sa setPluginIID()
*/
QString PluginManager::pluginIID()
{
    return d->pluginIID;
}

/*!
设置IID，即有效的插件必须有一个IID。只有插件与此IID被加载，其他的被忽略。
此时必须在调用setPluginPaths()之前调用它。
    \omit
    ///TODO 当loadPlugins()或plugins()被调用时，让这个 setPluginPaths轻松地读取插件元数据
    \endomit
*/
void PluginManager::setPluginIID(const QString &iid)
{
    d->pluginIID = iid;
}

/*!
定义要用于有关启用和的信息的用户特定设置禁用插件。
在使用setPluginPaths()设置插件搜索路径之前需要设置。
*/
void PluginManager::setSettings(QSettings *settings)
{
    d->setSettings(settings);
}

/*!
为有关的信息定义要使用的全局(用户独立的)设置,默认禁用插件。
在使用setPluginPaths()设置插件搜索路径之前需要设置。
*/
void PluginManager::setGlobalSettings(QSettings *settings)
{
    d->setGlobalSettings(settings);
}

/*!
返回用于有关启用和的信息的用户特定设置,默认禁用插件。
*/
QSettings *PluginManager::settings()
{
    return d->settings;
}

/*!
    返回用于默认禁用插件信息的全局(用户独立)设置。
*/
QSettings *PluginManager::globalSettings()
{
    return d->globalSettings;
}

void PluginManager::writeSettings()
{
    d->writeSettings();
}

/*!
解析剩下的参数(既不是启动也不是插件参数)。通常，这将是要打开的文件列表。
*/
QStringList PluginManager::arguments()
{
    return d->arguments;
}

/*!
自动重新启动应用程序时应该使用的参数。这包括用于启用或禁用插件的插件管理器相关选项，但是排除了其他参数，
比如arguments()和appOptions返回的参数传递给parseOptions()方法。
*/
QStringList PluginManager::argumentsForRestart()
{
    return d->argumentsForRestart;
}

/*!
在插件搜索路径中找到的所有插件的列表。这个列表在setPluginPaths()调用之后直接有效。
插件规范包含插件元数据和当前状态的插件。如果一个插件的库已经成功加载，插件规范也有一个创建插件实例的引用。

    \sa setPluginPaths()
*/
const QVector<PluginSpec *> PluginManager::plugins()
{
    return d->pluginSpecs;
}

QHash<QString, QVector<PluginSpec *>> PluginManager::pluginCollections()
{
    return d->pluginCategories;
}

static const char argumentKeywordC[] = ":arguments";
static const char pwdKeywordC[] = ":pwd";

/*!
序列化用于发送单个字符串的插件选项和参数
通过QtSingleApplication:    ":myplugin|-option1|-option2|:arguments|argument1|argument2",
作为以冒号关键字开头的列表列表。参数是最后一次。

    \sa setPluginPaths()
*/
QString PluginManager::serializedArguments()
{
    const QChar separator = QLatin1Char('|');
    QString rc;
    for (const PluginSpec *ps : plugins()) {
        if (!ps->arguments().isEmpty()) {
            if (!rc.isEmpty())
                rc += separator;
            rc += QLatin1Char(':');
            rc += ps->name();
            rc += separator;
            rc +=  ps->arguments().join(separator);
        }
    }
    if (!rc.isEmpty())
        rc += separator;
    rc += QLatin1String(pwdKeywordC) + separator + QDir::currentPath();
    if (!d->arguments.isEmpty()) {
        if (!rc.isEmpty())
            rc += separator;
        rc += QLatin1String(argumentKeywordC);
        for (const QString &argument : qAsConst(d->arguments))
            rc += separator + argument;
    }
    return rc;
}

/*!
以冒号开头的关键字表示从序列化的参数中提取子列表,
以冒号开头的关键字表示:":a,i1,i2,:b:i3,i4" 和 ":a" -> "i1,i2"
 */
static QStringList subList(const QStringList &in, const QString &key)
{
    QStringList rc;
    // 查找关键字并复制参数，直到end或next关键字
    const QStringList::const_iterator inEnd = in.constEnd();
    QStringList::const_iterator it = std::find(in.constBegin(), inEnd, key);
    if (it != inEnd) {
        const QChar nextIndicator = QLatin1Char(':');
        for (++it; it != inEnd && !it->startsWith(nextIndicator); ++it)
            rc.append(*it);
    }
    return rc;
}

/*!
解析在序列化参数中编码的选项并将它们与参数一起传递给相应的插件。
传递一个套接字，用于在操作完成时断开对等端连接(例如，文档已关闭)，以支持-block标志。
*/

void PluginManager::remoteArguments(const QString &serializedArgument, QObject *socket)
{
    if (serializedArgument.isEmpty())
        return;
    QStringList serializedArguments = serializedArgument.split(QLatin1Char('|'));
    const QStringList pwdValue = subList(serializedArguments, QLatin1String(pwdKeywordC));
    const QString workingDirectory = pwdValue.isEmpty() ? QString() : pwdValue.first();
    const QStringList arguments = subList(serializedArguments, QLatin1String(argumentKeywordC));
    for (const PluginSpec *ps : plugins()) {
        if (ps->state() == PluginSpec::Running) {
            const QStringList pluginOptions = subList(serializedArguments, QLatin1Char(':') + ps->name());
            QObject *socketParent = ps->plugin()->remoteCommand(pluginOptions, workingDirectory,
                                                                arguments);
            if (socketParent && socket) {
                socket->setParent(socketParent);
                socket = nullptr;
            }
        }
    }
    if (socket)
        delete socket;
}

/*! 
获取 args中的命令行选项列表并解析它们。
插件管理器本身可能直接处理一些选项( {-noload <plugin>})，并添加被注册的选项插件到他们的插件规格。
调用者(应用程序)可以通过一个appOptions列表，包含对{option string}和一个bool指示选项是否需要参数。
应用程序选项总是覆盖任何插件的选项。一个foundAppOptions被设置为对({option string}， 参数)
 */
bool PluginManager::parseOptions(const QStringList &args,
    const QMap<QString, bool> &appOptions,
    QMap<QString, QString> *foundAppOptions,
    QString *errorString)
{
    OptionsParser options(args, appOptions, foundAppOptions, errorString, d);
    return options.parse();
}



static inline void indent(QTextStream &str, int indent)
{
    str << QString(indent, ' ');
}

static inline void formatOption(QTextStream &str,
                                const QString &opt, const QString &parm, const QString &description,
                                int optionIndentation, int descriptionIndentation)
{
    int remainingIndent = descriptionIndentation - optionIndentation - opt.size();
    indent(str, optionIndentation);
    str << opt;
    if (!parm.isEmpty()) {
        str << " <" << parm << '>';
        remainingIndent -= 3 + parm.size();
    }
    if (remainingIndent >= 1) {
        indent(str, remainingIndent);
    } else {
        str << '\n';
        indent(str, descriptionIndentation);
    }
    str << description << '\n';
}

/*!
使用指定的命令行帮助格式化插件管理器的启动选项一个选项缩进和一个描述缩进。
将结果添加到一个str中。
*/

void PluginManager::formatOptions(QTextStream &str, int optionIndentation, int descriptionIndentation)
{
    formatOption(str, QLatin1String(OptionsParser::LOAD_OPTION),
                 QLatin1String("plugin"), QLatin1String("Load <plugin> and all plugins that it requires"),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QLatin1String(OptionsParser::LOAD_OPTION) + QLatin1String(" all"),
                 QString(), QLatin1String("Load all available plugins"),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QLatin1String(OptionsParser::NO_LOAD_OPTION),
                 QLatin1String("plugin"), QLatin1String("Do not load <plugin> and all plugins that require it"),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QLatin1String(OptionsParser::NO_LOAD_OPTION) + QLatin1String(" all"),
                 QString(), QString::fromLatin1("Do not load any plugin (useful when "
                                                "followed by one or more \"%1\" arguments)")
                 .arg(QLatin1String(OptionsParser::LOAD_OPTION)),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QLatin1String(OptionsParser::PROFILE_OPTION),
                 QString(), QLatin1String("Profile plugin loading"),
                 optionIndentation, descriptionIndentation);
    formatOption(str,
                 QLatin1String(OptionsParser::NO_CRASHCHECK_OPTION),
                 QString(),
                 QLatin1String("Disable startup check for previously crashed instance"),
                 optionIndentation,
                 descriptionIndentation);
#ifdef WITH_TESTS
    formatOption(str, QString::fromLatin1(OptionsParser::TEST_OPTION)
                 + QLatin1String(" <plugin>[,testfunction[:testdata]]..."), QString(),
                 QLatin1String("Run plugin's tests (by default a separate settings path is used)"),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QString::fromLatin1(OptionsParser::TEST_OPTION) + QLatin1String(" all"),
                 QString(), QLatin1String("Run tests from all plugins"),
                 optionIndentation, descriptionIndentation);
    formatOption(str, QString::fromLatin1(OptionsParser::NOTEST_OPTION),
                 QLatin1String("plugin"), QLatin1String("Exclude all of the plugin's tests from the test run"),
                 optionIndentation, descriptionIndentation);
#endif
}

/*!
为命令行帮助格式化插件规范的插件选项一个选项缩进和一个描述缩进。将结果添加到一个str中。
*/

void PluginManager::formatPluginOptions(QTextStream &str, int optionIndentation, int descriptionIndentation)
{
    // 检查插件选项
    for (PluginSpec *ps : qAsConst(d->pluginSpecs)) {
        const PluginSpec::PluginArgumentDescriptions pargs = ps->argumentDescriptions();
        if (!pargs.empty()) {
            str << "\nPlugin: " <<  ps->name() << '\n';
            for (const PluginArgumentDescription &pad : pargs)
                formatOption(str, pad.name, pad.parameter, pad.description, optionIndentation, descriptionIndentation);
        }
    }
}

/*!
格式化用于命令行帮助的插件规范的版本并将其添加到一个str中。
*/
void PluginManager::formatPluginVersions(QTextStream &str)
{
    for (PluginSpec *ps : qAsConst(d->pluginSpecs))
        str << "  " << ps->name() << ' ' << ps->version() << ' ' << ps->description() <<  '\n';
}

/*!
    \internal
 */
bool PluginManager::testRunRequested()
{
    return !d->testSpecs.empty();
}

/*!
    \internal
*/

void PluginManager::profilingReport(const char *what, const PluginSpec *spec)
{
    d->profilingReport(what, spec);
}


/*!
按加载顺序返回插件列表。
*/
QVector<PluginSpec *> PluginManager::loadQueue()
{
    return d->loadQueue();
}

//============PluginManagerPrivate===========

/*!
    \internal
*/
PluginSpec *PluginManagerPrivate::createSpec()
{
    return new PluginSpec();
}

/*!
    \internal
*/
void PluginManagerPrivate::setSettings(QSettings *s)
{
    if (settings)
        delete settings;
    settings = s;
    if (settings)
        settings->setParent(this);
}

/*!
    \internal
*/
void PluginManagerPrivate::setGlobalSettings(QSettings *s)
{
    if (globalSettings)
        delete globalSettings;
    globalSettings = s;
    if (globalSettings)
        globalSettings->setParent(this);
}

/*!
    \internal
*/
PluginSpecPrivate *PluginManagerPrivate::privateSpec(PluginSpec *spec)
{
    return spec->d;
}

void PluginManagerPrivate::nextDelayedInitialize()
{
    while (!delayedInitializeQueue.empty()) {
        PluginSpec *spec = delayedInitializeQueue.front();
        delayedInitializeQueue.pop();
        profilingReport(">delayedInitialize", spec);
        bool delay = spec->d->delayedInitialize();
        profilingReport("<delayedInitialize", spec);
        if (delay)
            break; // 做下一个延迟后的延迟初始化
    }
    if (delayedInitializeQueue.empty()) {
        m_isInitializationDone = true;
        delete delayedInitializeTimer;
        delayedInitializeTimer = nullptr;
        profilingSummary();
        emit q->initializationDone();
#ifdef WITH_TESTS
        if (q->testRunRequested())
            startTests();
#endif
    } else {
        delayedInitializeTimer->start();
    }
}

/*!
    \internal
*/
PluginManagerPrivate::PluginManagerPrivate(PluginManager *pluginManager) :
    q(pluginManager)
{
}


/*!
    \internal
*/
PluginManagerPrivate::~PluginManagerPrivate()
{
    qDeleteAll(pluginSpecs);
}

/*!
    \internal
*/
void PluginManagerPrivate::writeSettings()
{
    if (!settings)
        return;
    QStringList tempDisabledPlugins;
    QStringList tempForceEnabledPlugins;
    for (PluginSpec *spec : qAsConst(pluginSpecs)) {
        if (spec->isEnabledByDefault() && !spec->isEnabledBySettings())
            tempDisabledPlugins.append(spec->name());
        if (!spec->isEnabledByDefault() && spec->isEnabledBySettings())
            tempForceEnabledPlugins.append(spec->name());
    }

    settings->setValue(QLatin1String(C_IGNORED_PLUGINS), tempDisabledPlugins);
    settings->setValue(QLatin1String(C_FORCEENABLED_PLUGINS), tempForceEnabledPlugins);
}

/*!
    \internal
*/
void PluginManagerPrivate::readSettings()
{
    if (globalSettings) {
        defaultDisabledPlugins = globalSettings->value(QLatin1String(C_IGNORED_PLUGINS)).toStringList();
        defaultEnabledPlugins = globalSettings->value(QLatin1String(C_FORCEENABLED_PLUGINS)).toStringList();
    }
    if (settings) {
        disabledPlugins = settings->value(QLatin1String(C_IGNORED_PLUGINS)).toStringList();
        forceEnabledPlugins = settings->value(QLatin1String(C_FORCEENABLED_PLUGINS)).toStringList();
    }
}

/*!
    \internal
*/
void PluginManagerPrivate::stopAll()
{
    if (delayedInitializeTimer && delayedInitializeTimer->isActive()) {
        delayedInitializeTimer->stop();
        delete delayedInitializeTimer;
        delayedInitializeTimer = nullptr;
    }

    const QVector<PluginSpec *> queue = loadQueue();
    for (PluginSpec *spec : queue)
        loadPlugin(spec, PluginSpec::Stopped);
}

/*!
    \internal
*/
void PluginManagerPrivate::deleteAll()
{
    Utils::reverseForeach(loadQueue(), [this](PluginSpec *spec) {
        loadPlugin(spec, PluginSpec::Deleted);
    });
}

#ifdef WITH_TESTS

using TestPlan = QMap<QObject *, QStringList>; // Object -> selected test functions 对象->选择测试函数

static bool isTestFunction(const QMetaMethod &metaMethod)
{
    static const QVector<QByteArray> blackList = {"initTestCase()",
                                                  "cleanupTestCase()",
                                                  "init()",
                                                  "cleanup()"};

    if (metaMethod.methodType() != QMetaMethod::Slot)
        return false;

    if (metaMethod.access() != QMetaMethod::Private)
        return false;

    const QByteArray signature = metaMethod.methodSignature();
    if (blackList.contains(signature))
        return false;

    if (!signature.startsWith("test"))
        return false;

    if (signature.endsWith("_data()"))
        return false;

    return true;
}

static QStringList testFunctions(const QMetaObject *metaObject)
{

    QStringList functions;

    for (int i = metaObject->methodOffset(); i < metaObject->methodCount(); ++i) {
        const QMetaMethod metaMethod = metaObject->method(i);
        if (isTestFunction(metaMethod)) {
            const QByteArray signature = metaMethod.methodSignature();
            const QString method = QString::fromLatin1(signature);
            const QString methodName = method.left(method.size() - 2);
            functions.append(methodName);
        }
    }

    return functions;
}

static QStringList matchingTestFunctions(const QStringList &testFunctions,
                                         const QString &matchText)
{
    // 可能会有一个像“testfunction:testdata1”这样的测试数据后缀
    QString testFunctionName = matchText;
    QString testDataSuffix;
    const int index = testFunctionName.indexOf(QLatin1Char(':'));
    if (index != -1) {
        testDataSuffix = testFunctionName.mid(index);
        testFunctionName = testFunctionName.left(index);
    }

    const QRegularExpression regExp(
                QRegularExpression::wildcardToRegularExpression(testFunctionName));
    QStringList matchingFunctions;
    for (const QString &testFunction : testFunctions) {
        if (regExp.match(testFunction).hasMatch()) {
            //如果指定的测试数据无效，QTest框架将会
            //为我们打印一个合理的错误信息。
            matchingFunctions.append(testFunction + testDataSuffix);
        }
    }

    return matchingFunctions;
}

static QObject *objectWithClassName(const QVector<QObject *> &objects, const QString &className)
{
    return Utils::findOr(objects, nullptr, [className] (QObject *object) -> bool {
        QString candidate = QString::fromUtf8(object->metaObject()->className());
        const int colonIndex = candidate.lastIndexOf(QLatin1Char(':'));
        if (colonIndex != -1 && colonIndex < candidate.size() - 1)
            candidate = candidate.mid(colonIndex + 1);
        return candidate == className;
    });
}

static int executeTestPlan(const TestPlan &testPlan)
{
    int failedTests = 0;

    for (auto it = testPlan.cbegin(), end = testPlan.cend(); it != end; ++it) {
        QObject *testObject = it.key();
        QStringList functions = it.value();

        // 不要在没有任何测试函数的情况下运行QTest::qExec，这些函数将运行*所有*插槽作为测试。
        if (functions.isEmpty())
            continue;

        functions.removeDuplicates();

        // QTest::qExec()基本上期望QCoreApplication::arguments()，
        QStringList qExecArguments = QStringList()
                << QLatin1String("arg0") // 假的应用程序名称
                << QLatin1String("-maxwarnings") << QLatin1String("0"); // 不限制 输出
        qExecArguments << functions;
        // 避免陷入困境在 QTBUG-24925
        if (!HostOsInfo::isWindowsHost())
            qExecArguments << "-nocrashhandler";
        failedTests += QTest::qExec(testObject, qExecArguments);
    }

    return failedTests;
}

///生成的计划由插件对象的所有测试函数和插件所有测试对象的所有测试函数。
static TestPlan generateCompleteTestPlan(IPlugin *plugin, const QVector<QObject *> &testObjects)
{
    TestPlan testPlan;

    testPlan.insert(plugin, testFunctions(plugin->metaObject()));
    for (QObject *testObject : testObjects) {
        const QStringList allFunctions = testFunctions(testObject->metaObject());
        testPlan.insert(testObject, allFunctions);
    }

    return testPlan;
}

/*!
生成的计划由插件对象的所有匹配测试函数和插件所有测试对象的所有匹配函数组成。
但是，如果匹配文本表示一个测试类，则该测试类的所有测试函数将被包含在中，并且该类将不再被进一步考虑。
由于多个匹配文本可以匹配相同的函数，因此测试函数可能被多次包含在一个测试对象中。
*/
static TestPlan generateCustomTestPlan(IPlugin *plugin,
                                       const QVector<QObject *> &testObjects,
                                       const QStringList &matchTexts)
{
    TestPlan testPlan;

    const QStringList testFunctionsOfPluginObject = testFunctions(plugin->metaObject());
    QStringList matchedTestFunctionsOfPluginObject;
    QStringList remainingMatchTexts = matchTexts;
    QVector<QObject *> remainingTestObjectsOfPlugin = testObjects;

    while (!remainingMatchTexts.isEmpty()) {
        const QString matchText = remainingMatchTexts.takeFirst();
        bool matched = false;

        if (QObject *testObject = objectWithClassName(remainingTestObjectsOfPlugin, matchText)) {
            /// 添加匹配测试对象的所有功能
            matched = true;
            testPlan.insert(testObject, testFunctions(testObject->metaObject()));
            remainingTestObjectsOfPlugin.removeAll(testObject);

        } else {
            /// 添加所有剩余测试对象的所有匹配测试功能
            for (QObject *testObject : qAsConst(remainingTestObjectsOfPlugin)) {
                const QStringList allFunctions = testFunctions(testObject->metaObject());
                const QStringList matchingFunctions = matchingTestFunctions(allFunctions,
                                                                            matchText);
                if (!matchingFunctions.isEmpty()) {
                    matched = true;
                    testPlan[testObject] += matchingFunctions;
                }
            }
        }

        const QStringList currentMatchedTestFunctionsOfPluginObject
            = matchingTestFunctions(testFunctionsOfPluginObject, matchText);
        if (!currentMatchedTestFunctionsOfPluginObject.isEmpty()) {
            matched = true;
            matchedTestFunctionsOfPluginObject += currentMatchedTestFunctionsOfPluginObject;
        }

        if (!matched) {
            QTextStream out(stdout);
            out << "没有测试函数或类匹配 \"" << matchText
                << "\" 在插件 \"" << plugin->metaObject()->className()
                << "\".\n可用功能:\n";
            for (const QString &f : testFunctionsOfPluginObject)
                out << "  " << f << '\n';
            out << '\n';
        }
    }

    /// 添加插件的所有匹配测试函数
    if (!matchedTestFunctionsOfPluginObject.isEmpty())
        testPlan.insert(plugin, matchedTestFunctionsOfPluginObject);

    return testPlan;
}

void PluginManagerPrivate::startTests()
{
    if (PluginManager::hasError()) {
        qWarning("Errors occurred while loading plugins, skipping test run.");
        for (const QString &pluginError : PluginManager::allErrors())
            qWarning("%s", qPrintable(pluginError));
        QTimer::singleShot(1, QCoreApplication::instance(), &QCoreApplication::quit);
        return;
    }

    int failedTests = 0;
    for (const TestSpec &testSpec : qAsConst(testSpecs)) {
        IPlugin *plugin = testSpec.pluginSpec->plugin();
        if (!plugin)
            continue; //插件不加载

        const QVector<QObject *> testObjects = plugin->createTestObjects();
        ExecuteOnDestruction deleteTestObjects([&]() { qDeleteAll(testObjects); });
        Q_UNUSED(deleteTestObjects)

        const bool hasDuplicateTestObjects = testObjects.size()
                                             != Utils::filteredUnique(testObjects).size();
        QTC_ASSERT(!hasDuplicateTestObjects, continue);
        QTC_ASSERT(!testObjects.contains(plugin), continue);

        const TestPlan testPlan = testSpec.testFunctionsOrObjects.isEmpty()
                ? generateCompleteTestPlan(plugin, testObjects)
                : generateCustomTestPlan(plugin, testObjects, testSpec.testFunctionsOrObjects);

        failedTests += executeTestPlan(testPlan);
    }

    QTimer::singleShot(0, this, [failedTests]() { emit m_instance->testsFinished(failedTests); });
}
#endif

/*!
    \internal
*/
void PluginManagerPrivate::addObject(QObject *obj)
{
    {
        QWriteLocker lock(&m_lock);
        if (obj == nullptr) {
            qWarning() << "PluginManagerPrivate::addObject(): 试图添加空对象";
            return;
        }
        if (allObjects.contains(obj)) {
            qWarning() << "PluginManagerPrivate::addObject(): 试图添加复制对象";
            return;
        }

        if (debugLeaks)
            qDebug() << "PluginManagerPrivate::addObject" << obj << obj->objectName();

        if (m_profilingVerbosity && !m_profileTimer.isNull()) {
            //在添加对象时报告时间戳。用于分析初始化时间。
            const int absoluteElapsedMS = int(m_profileTimer->elapsed());
            qDebug("  %-43s %8dms", obj->metaObject()->className(), absoluteElapsedMS);
        }

        allObjects.append(obj);
    }
    emit q->objectAdded(obj);
}

/*!
    \internal
*/
void PluginManagerPrivate::removeObject(QObject *obj)
{
    if (obj == nullptr) {
        qWarning() << "PluginManagerPrivate::removeObject(): 试图删除空对象";
        return;
    }

    if (!allObjects.contains(obj)) {
        qWarning() << "PluginManagerPrivate::removeObject(): 未在列表中的对象:"<< obj << obj->objectName();
        return;
    }
    if (debugLeaks)
        qDebug() << "PluginManagerPrivate::removeObject" << obj << obj->objectName();

    emit q->aboutToRemoveObject(obj);
    QWriteLocker lock(&m_lock);
    allObjects.removeAll(obj);
}

/*!
    \internal
*/
void PluginManagerPrivate::loadPlugins()
{
    const QVector<PluginSpec *> queue = loadQueue();
    Utils::setMimeStartupPhase(MimeStartupPhase::PluginsLoading);
    for (PluginSpec *spec : queue)
        loadPlugin(spec, PluginSpec::Loaded);

    Utils::setMimeStartupPhase(MimeStartupPhase::PluginsInitializing);
    for (PluginSpec *spec : queue)
        loadPlugin(spec, PluginSpec::Initialized);

    Utils::setMimeStartupPhase(MimeStartupPhase::PluginsDelayedInitializing);
    Utils::reverseForeach(queue, [this](PluginSpec *spec) {
        loadPlugin(spec, PluginSpec::Running);
        if (spec->state() == PluginSpec::Running) {
            delayedInitializeQueue.push(spec);
        } else {
            // 插件初始化失败，因此在它之后进行清理
            spec->d->kill();
        }
    });
    emit q->pluginsChanged();
    Utils::setMimeStartupPhase(MimeStartupPhase::UpAndRunning);

    delayedInitializeTimer = new QTimer;
    delayedInitializeTimer->setInterval(DELAYED_INITIALIZE_INTERVAL);
    delayedInitializeTimer->setSingleShot(true);
    connect(delayedInitializeTimer, &QTimer::timeout,
            this, &PluginManagerPrivate::nextDelayedInitialize);
    delayedInitializeTimer->start();
}

/*!
    \internal
*/
void PluginManagerPrivate::shutdown()
{
    stopAll();
    if (!asynchronousPlugins.isEmpty()) {
        shutdownEventLoop = new QEventLoop;
        shutdownEventLoop->exec();
    }
    deleteAll();
    if (!allObjects.isEmpty()) {
        qDebug() << "有" << allObjects.size() << "个保留在插件管理器池中的对象.";
        ///故意在这里分割调试信息，因为万一列表包含已经删除的对象，至少得到对象数量的信息;
        qDebug() << "以下对象留在插件管理器池中:" << allObjects;
    }
}

/*!
    \internal
*/
void PluginManagerPrivate::asyncShutdownFinished()
{
    auto *plugin = qobject_cast<IPlugin *>(sender());
    Q_ASSERT(plugin);
    asynchronousPlugins.remove(plugin->pluginSpec());
    if (asynchronousPlugins.isEmpty())
        shutdownEventLoop->exit();
}

/*!
    \internal
*/
const QVector<PluginSpec *> PluginManagerPrivate::loadQueue()
{
    QVector<PluginSpec *> queue;
    for (PluginSpec *spec : qAsConst(pluginSpecs)) {
        QVector<PluginSpec *> circularityCheckQueue;
        loadQueue(spec, queue, circularityCheckQueue);
    }
    return queue;
}

/*!
    \internal
*/
bool PluginManagerPrivate::loadQueue(PluginSpec *spec,
                                     QVector<PluginSpec *> &queue,
                                     QVector<PluginSpec *> &circularityCheckQueue)
{
    if (queue.contains(spec))
        return true;
    // 检查循环依赖关系
    if (circularityCheckQueue.contains(spec)) {
        spec->d->hasError = true;
        spec->d->errorString = PluginManager::tr("Circular dependency detected:");
        spec->d->errorString += QLatin1Char('\n');
        int index = circularityCheckQueue.indexOf(spec);
        for (int i = index; i < circularityCheckQueue.size(); ++i) {
            spec->d->errorString.append(PluginManager::tr("%1 (%2) depends on")
                .arg(circularityCheckQueue.at(i)->name()).arg(circularityCheckQueue.at(i)->version()));
            spec->d->errorString += QLatin1Char('\n');
        }
        spec->d->errorString.append(PluginManager::tr("%1 (%2)").arg(spec->name()).arg(spec->version()));
        return false;
    }
    circularityCheckQueue.append(spec);
    //检查我们是否有依赖关系
    if (spec->state() == PluginSpec::Invalid || spec->state() == PluginSpec::Read) {
        queue.append(spec);
        return false;
    }

    // 添加依赖关系
    const QHash<PluginDependency, PluginSpec *> deps = spec->dependencySpecs();
    for (auto it = deps.cbegin(), end = deps.cend(); it != end; ++it) {
        //跳过测试依赖关系，因为它们不是真正的依赖关系，只是强制加载的运行测试时的插件
        if (it.key().type == PluginDependency::Test)
            continue;
        PluginSpec *depSpec = it.value();
        if (!loadQueue(depSpec, queue, circularityCheckQueue)) {
            spec->d->hasError = true;
            spec->d->errorString =
                PluginManager::tr("Cannot load plugin because dependency failed to load: %1 (%2)\nReason: %3")
                    .arg(depSpec->name()).arg(depSpec->version()).arg(depSpec->errorString());
            return false;
        }
    }
    // 增加自己
    queue.append(spec);
    return true;
}

class LockFile
{
public:
    static QString filePath(PluginManagerPrivate *pm)
    {
        return QFileInfo(pm->settings->fileName()).absolutePath() + '/'
               + QCoreApplication::applicationName() + '.'
               + QCryptographicHash::hash(QCoreApplication::applicationDirPath().toUtf8(),
                                          QCryptographicHash::Sha1)
                     .left(8)
                     .toHex()
               + ".lock";
    }

    static Utils::optional<QString> lockedPluginName(PluginManagerPrivate *pm)
    {
        const QString lockFilePath = LockFile::filePath(pm);
        if (QFile::exists(lockFilePath)) {
            QFile f(lockFilePath);
            if (f.open(QIODevice::ReadOnly)) {
                const auto pluginName = QString::fromUtf8(f.readLine()).trimmed();
                f.close();
                return pluginName;
            } else {
                qCDebug(pluginLog) << "锁定文件" << lockFilePath << "存在，但不可读";
            }
        }
        return {};
    }

    LockFile(PluginManagerPrivate *pm, PluginSpec *spec) : m_filePath(filePath(pm))
    {
        QDir().mkpath(QFileInfo(m_filePath).absolutePath());
        QFile f(m_filePath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(spec->name().toUtf8());
            f.write("\n");
            f.close();
        } else {
            qCDebug(pluginLog) << "无法写入锁定文件" << m_filePath;
        }
    }

    ~LockFile() { QFile::remove(m_filePath); }

private:
    QString m_filePath;
};

void PluginManagerPrivate::checkForProblematicPlugins()
{
    if (!enableCrashCheck)
        return;
    const Utils::optional<QString> pluginName = LockFile::lockedPluginName(this);
    if (pluginName) {
        PluginSpec *spec = pluginByName(*pluginName);
        if (spec && !spec->isRequired()) {
            const QSet<PluginSpec *> dependents = PluginManager::pluginsRequiringPlugin(spec);
            auto dependentsNames = Utils::transform<QStringList>(dependents, &PluginSpec::name);
            std::sort(dependentsNames.begin(), dependentsNames.end());
            const QString dependentsList = dependentsNames.join(", ");
            const QString pluginsMenu = HostOsInfo::isMacHost()
                                            ? tr("%1 > About Plugins")
                                                  .arg(QGuiApplication::applicationDisplayName())
                                            : tr("Help > About Plugins");
            const QString otherPluginsText = tr("The following plugins depend on "
                                                "%1 and are also disabled: %2.\n\n")
                                                 .arg(spec->name(), dependentsList);
            const QString detailsText = (dependents.isEmpty() ? QString() : otherPluginsText)
                                        + tr("Disable plugins permanently in %1.").arg(pluginsMenu);
            const QString text = tr("It looks like %1 closed because of a problem with the \"%2\" "
                                    "plugin. Temporarily disable the plugin?")
                                     .arg(QGuiApplication::applicationDisplayName(), spec->name());
            QMessageBox dialog;
            dialog.setIcon(QMessageBox::Question);
            dialog.setText(text);
            dialog.setDetailedText(detailsText);
            QPushButton *disableButton = dialog.addButton(tr("Disable Plugin"),
                                                          QMessageBox::AcceptRole);
            dialog.addButton(tr("Continue"), QMessageBox::RejectRole);
            dialog.exec();
            if (dialog.clickedButton() == disableButton) {
                spec->d->setForceDisabled(true);
                for (PluginSpec *other : dependents)
                    other->d->setForceDisabled(true);
                enableDependenciesIndirectly();
            }
        }
    }
}

void PluginManager::checkForProblematicPlugins()
{
    d->checkForProblematicPlugins();
}

/*!
    \internal
*/
void PluginManagerPrivate::loadPlugin(PluginSpec *spec, PluginSpec::State destState)
{
    if (spec->hasError() || spec->state() != destState-1)
        return;

    // 不要加载禁用的插件。
    if (!spec->isEffectivelyEnabled() && destState == PluginSpec::Loaded)
        return;

    std::unique_ptr<LockFile> lockFile;
    if (enableCrashCheck)
        lockFile.reset(new LockFile(this, spec));

    switch (destState) {
    case PluginSpec::Running:
        profilingReport(">初始化扩展", spec);
        spec->d->initializeExtensions();
        profilingReport("<初始化扩展", spec);
        return;
    case PluginSpec::Deleted:
        profilingReport(">删除", spec);
        spec->d->kill();
        profilingReport("<删除", spec);
        return;
    default:
        break;
    }
    // 检查依赖项是否加载无误
    const QHash<PluginDependency, PluginSpec *> deps = spec->dependencySpecs();
    for (auto it = deps.cbegin(), end = deps.cend(); it != end; ++it) {
        if (it.key().type != PluginDependency::Required)
            continue;
        PluginSpec *depSpec = it.value();
        if (depSpec->state() != destState) {
            spec->d->hasError = true;
            spec->d->errorString =
                PluginManager::tr("Cannot load plugin because dependency failed to load: %1(%2)\nReason: %3")
                    .arg(depSpec->name()).arg(depSpec->version()).arg(depSpec->errorString());
            return;
        }
    }
    switch (destState) {
    case PluginSpec::Loaded:
        profilingReport(">加载库", spec);
        spec->d->loadLibrary();
        profilingReport("<加载库", spec);
        break;
    case PluginSpec::Initialized:
        profilingReport(">初始化插件", spec);
        spec->d->initializePlugin();
        profilingReport("<初始化插件", spec);
        break;
    case PluginSpec::Stopped:
        profilingReport(">停止", spec);
        if (spec->d->stop() == IPlugin::AsynchronousShutdown) {
            asynchronousPlugins << spec;
            connect(spec->plugin(), &IPlugin::asynchronousShutdownFinished,
                    this, &PluginManagerPrivate::asyncShutdownFinished);
        }
        profilingReport("<停止", spec);
        break;
    default:
        break;
    }
}

/*!
    \internal
*/
void PluginManagerPrivate::setPluginPaths(const QStringList &paths)
{
    qCDebug(pluginLog) << "插件搜索路径:" << paths;
    qCDebug(pluginLog) << "必需的 IID:" << pluginIID;
    pluginPaths = paths;
    readSettings();
    readPluginPaths();
}

static const QStringList pluginFiles(const QStringList &pluginPaths)
{
    QStringList pluginFiles;
    QStringList searchPaths = pluginPaths;
    while (!searchPaths.isEmpty()) {
        const QDir dir(searchPaths.takeFirst());
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);
        const QStringList absoluteFilePaths = Utils::transform(files, &QFileInfo::absoluteFilePath);
        pluginFiles += Utils::filtered(absoluteFilePaths, [](const QString &path) { return QLibrary::isLibrary(path); });
        const QFileInfoList dirs = dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot);
        searchPaths += Utils::transform(dirs, &QFileInfo::absoluteFilePath);
    }
    return pluginFiles;
}

/*!
    \internal
*/
void PluginManagerPrivate::readPluginPaths()
{
    qDeleteAll(pluginSpecs);
    pluginSpecs.clear();
    pluginCategories.clear();

    // 默认
    pluginCategories.insert(QString(), QVector<PluginSpec *>());

    for (const QString &pluginFile : pluginFiles(pluginPaths)) {
        PluginSpec *spec = PluginSpec::read(pluginFile);
        if (!spec) // 不是Qt Creator插件
            continue;

        // defaultDisabledPlugins和defaultEnabledPlugins从安装设置用于覆盖从插件规范中读取的默认值
        if (spec->isEnabledByDefault() && defaultDisabledPlugins.contains(spec->name())) {
            spec->d->setEnabledByDefault(false);
            spec->d->setEnabledBySettings(false);
        } else if (!spec->isEnabledByDefault() && defaultEnabledPlugins.contains(spec->name())) {
            spec->d->setEnabledByDefault(true);
            spec->d->setEnabledBySettings(true);
        }
        if (!spec->isEnabledByDefault() && forceEnabledPlugins.contains(spec->name()))
            spec->d->setEnabledBySettings(true);
        if (spec->isEnabledByDefault() && disabledPlugins.contains(spec->name()))
            spec->d->setEnabledBySettings(false);

        pluginCategories[spec->category()].append(spec);
        pluginSpecs.append(spec);
    }
    resolveDependencies();
    enableDependenciesIndirectly();
    // 通过排序确保确定性插件加载顺序
    Utils::sort(pluginSpecs, &PluginSpec::name);
    emit q->pluginsChanged();
}

void PluginManagerPrivate::resolveDependencies()
{
    for (PluginSpec *spec : qAsConst(pluginSpecs))
        spec->d->resolveDependencies(pluginSpecs);
}

void PluginManagerPrivate::enableDependenciesIndirectly()
{
    for (PluginSpec *spec : qAsConst(pluginSpecs))
        spec->d->enabledIndirectly = false;
    // 这里不能使用反向加载队列，因为测试依赖关系可能会引入循环
    QVector<PluginSpec *> queue = Utils::filtered(pluginSpecs, &PluginSpec::isEffectivelyEnabled);
    while (!queue.isEmpty()) {
        PluginSpec *spec = queue.takeFirst();
        queue += spec->d->enableDependenciesIndirectly(containsTestSpec(spec));
    }
}

// 查看选项规范的参数描述。
PluginSpec *PluginManagerPrivate::pluginForOption(const QString &option, bool *requiresArgument) const
{
    // 在插件中寻找一个选项
    *requiresArgument = false;
    for (PluginSpec *spec : qAsConst(pluginSpecs)) {
        PluginArgumentDescription match = Utils::findOrDefault(spec->argumentDescriptions(),
                                                               [option](PluginArgumentDescription pad) {
                                                                   return pad.name == option;
                                                               });
        if (!match.name.isEmpty()) {
            *requiresArgument = !match.parameter.isEmpty();
            return spec;
        }
    }
    return nullptr;
}

PluginSpec *PluginManagerPrivate::pluginByName(const QString &name) const
{
    return Utils::findOrDefault(pluginSpecs, [name](PluginSpec *spec) { return spec->name() == name; });
}

void PluginManagerPrivate::initProfiling()
{
    if (m_profileTimer.isNull()) {
        m_profileTimer.reset(new QElapsedTimer);
        m_profileTimer->start();
        m_profileElapsedMS = 0;
        qDebug("分析开始");
    } else {
        m_profilingVerbosity++;
    }
}

void PluginManagerPrivate::profilingReport(const char *what, const PluginSpec *spec /* = 0 */)
{
    if (!m_profileTimer.isNull()) {
        const int absoluteElapsedMS = int(m_profileTimer->elapsed());
        const int elapsedMS = absoluteElapsedMS - m_profileElapsedMS;
        m_profileElapsedMS = absoluteElapsedMS;
        if (spec)
            qDebug("%-22s %-22s %8dms (%8dms)", what, qPrintable(spec->name()), absoluteElapsedMS, elapsedMS);
        else
            qDebug("%-45s %8dms (%8dms)", what, absoluteElapsedMS, elapsedMS);
        if (what && *what == '<') {
            QString tc;
            if (spec) {
                m_profileTotal[spec] += elapsedMS;
                tc = spec->name() + '_';
            }
            tc += QString::fromUtf8(QByteArray(what + 1));
            Utils::Benchmarker::report("加载插件", tc, elapsedMS);
        }
    }
}

void PluginManagerPrivate::profilingSummary() const
{
    if (!m_profileTimer.isNull()) {
        QMultiMap<int, const PluginSpec *> sorter;
        int total = 0;

        auto totalEnd = m_profileTotal.constEnd();
        for (auto it = m_profileTotal.constBegin(); it != totalEnd; ++it) {
            sorter.insert(it.value(), it.key());
            total += it.value();
        }

        auto sorterEnd = sorter.constEnd();
        for (auto it = sorter.constBegin(); it != sorterEnd; ++it)
            qDebug("%-22s %8dms   ( %5.2f%% )", qPrintable(it.value()->name()),
                it.key(), 100.0 * it.key() / total);
         qDebug("Total: %8dms", total);
         Utils::Benchmarker::report("loadPlugins", "Total", total);
    }
}

static inline QString getPlatformName()
{
    if (HostOsInfo::isMacHost())
        return QLatin1String("OS X");
    else if (HostOsInfo::isAnyUnixHost())
        return QLatin1String(HostOsInfo::isLinuxHost() ? "Linux" : "Unix");
    else if (HostOsInfo::isWindowsHost())
        return QLatin1String("Windows");
    return QLatin1String("Unknown");
}

QString PluginManager::platformName()
{
    static const QString result = getPlatformName() + " (" + QSysInfo::prettyProductName() + ')';
    return result;
}

bool PluginManager::isInitializationDone()
{
    return d->m_isInitializationDone;
}

/*!
    从对象池中检索一个的对象通过名称。
    \sa addObject()
*/

QObject *PluginManager::getObjectByName(const QString &name)
{
    QReadLocker lock(&d->m_lock);
    return Utils::findOrDefault(allObjects(), [&name](const QObject *obj) {
        return obj->objectName() == name;
    });
}

} // ExtensionSystem
