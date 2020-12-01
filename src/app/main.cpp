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

#include "../tools/qtcreatorcrashhandler/crashhandlersetup.h"

#include <app/app_version.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginerroroverview.h>
#include <extensionsystem/pluginmanager.h>
#include <extensionsystem/pluginspec.h>
#include <qtsingleapplication.h>

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/optional.h>
#include <utils/temporarydirectory.h>

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QSettings>
#include <QStyle>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVariant>

#include <QNetworkProxyFactory>

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <string>
#include <vector>
#include <iterator>

#ifdef ENABLE_QT_BREAKPAD
#include <qtsystemexceptionhandler.h>
#endif

#ifdef Q_OS_LINUX
#include <malloc.h>
#endif

using namespace ExtensionSystem;

enum { OptionIndent = 4, DescriptionIndent = 34 };

const char corePluginNameC[] = "Core";
const char fixedOptionsC[] =
" [OPTION]... [FILE]...\n"
"Options:\n"
"    -help                         Display this help\n"
"    -version                      Display program version\n"
"    -client                       Attempt to connect to already running first instance\n"
"    -settingspath <path>          Override the default path where user settings are stored\n"
"    -installsettingspath <path>   Override the default path from where user-independent settings are read\n"
"    -temporarycleansettings       Use clean settings for debug or testing reasons\n"
"    -pid <pid>                    Attempt to connect to instance given by pid\n"
"    -block                        Block until editor is closed\n"
"    -pluginpath <path>            Add a custom search path for plugins\n";

const char HELP_OPTION1[] = "-h";
const char HELP_OPTION2[] = "-help";
const char HELP_OPTION3[] = "/h";
const char HELP_OPTION4[] = "--help";
const char VERSION_OPTION[] = "-version";
const char CLIENT_OPTION[] = "-client";
const char SETTINGS_OPTION[] = "-settingspath";
const char INSTALL_SETTINGS_OPTION[] = "-installsettingspath";
const char TEST_OPTION[] = "-test";
const char TEMPORARY_CLEAN_SETTINGS1[] = "-temporarycleansettings";
const char TEMPORARY_CLEAN_SETTINGS2[] = "-tcs";
const char PID_OPTION[] = "-pid";
const char BLOCK_OPTION[] = "-block";
const char PLUGINPATH_OPTION[] = "-pluginpath";
const char USER_LIBRARY_PATH_OPTION[] = "-user-library-path"; // hidden option for qtcreator.sh

using PluginSpecSet = QVector<PluginSpec *>;

//显示消息的助手。注意，Windows上没有控制台。

// 格式化 as <pre> HTML
static inline QString toHtml(const QString &t)
{
    QString res = t;
    res.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    res.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    res.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    res.insert(0, QLatin1String("<html><pre>"));
    res.append(QLatin1String("</pre></html>"));
    return res;
}

static void displayHelpText(const QString &t)
{
    if (Utils::HostOsInfo::isWindowsHost() && qApp)
        QMessageBox::information(nullptr, QLatin1String(Core::Constants::IDE_DISPLAY_NAME), toHtml(t));
    else
        qWarning("%s", qPrintable(t));
}

static void displayError(const QString &t)
{
    if (Utils::HostOsInfo::isWindowsHost() && qApp)
        QMessageBox::critical(nullptr, QLatin1String(Core::Constants::IDE_DISPLAY_NAME), t);
    else
        qCritical("%s", qPrintable(t));
}

static void printVersion(const PluginSpec *coreplugin)
{
    QString version;
    QTextStream str(&version);
    str << '\n' << Core::Constants::IDE_DISPLAY_NAME << ' ' << coreplugin->version()<< " based on Qt " << qVersion() << "\n\n";
    PluginManager::formatPluginVersions(str);
    str << '\n' << coreplugin->copyright() << '\n';
    displayHelpText(version);
}

static void printHelp(const QString &a0)
{
    QString help;
    QTextStream str(&help);
    str << "Usage: " << a0 << fixedOptionsC;
    PluginManager::formatOptions(str, OptionIndent, DescriptionIndent);
    PluginManager::formatPluginOptions(str, OptionIndent, DescriptionIndent);
    displayHelpText(help);
}

QString applicationDirPath(char *arg = nullptr)
{
    static QString dir;

    if (arg)
        dir = QFileInfo(QString::fromLocal8Bit(arg)).dir().absolutePath();

    if (QCoreApplication::instance())
        return QApplication::applicationDirPath();

    return dir;
}

static QString resourcePath()
{
    return QDir::cleanPath(applicationDirPath() + '/' + RELATIVE_DATA_PATH);
}

static inline QString msgCoreLoadFailure(const QString &why)
{
    return QCoreApplication::translate("Application", "Failed to load core: %1").arg(why);
}
//请求消息发送失败
static inline int askMsgSendFailed()
{
    return QMessageBox::question(nullptr, QApplication::translate("Application","Could not send message"),
                QCoreApplication::translate("Application", "Unable to send command line arguments "
                                            "to the already running instance. It does not appear to "
                                            "be responding. Do you want to start a new instance of "
                                            "%1?").arg(Core::Constants::IDE_DISPLAY_NAME),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Retry,
                QMessageBox::Retry);
}

// taken from utils/fileutils.cpp. 我们不能在这里使用utils，因为它依赖于app_version.h。
static bool copyRecursively(const QString &srcFilePath, const QString &tgtFilePath)
{
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        QDir targetDir(tgtFilePath);
        targetDir.cdUp();
        if (!targetDir.mkdir(Utils::FilePath::fromString(tgtFilePath).fileName()))
            return false;
        QDir sourceDir(srcFilePath);
        const QStringList fileNames = sourceDir.entryList
                (QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QString &fileName : fileNames) {
            const QString newSrcFilePath = srcFilePath + '/' + fileName;
            const QString newTgtFilePath = tgtFilePath + '/' + fileName;
            if (!copyRecursively(newSrcFilePath, newTgtFilePath))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath))
            return false;
    }
    return true;
}

static inline QStringList getPluginPaths()
{
    QStringList rc(QDir::cleanPath(QApplication::applicationDirPath()
                                   + '/' + RELATIVE_PLUGIN_PATH));
    // 本地插件路径:: <localappdata>/plugins/<ideversion>
    //    其中<localappdata>为例,%LOCALAPPDATA%\QtProject\qtcreator"在windowsvista和以后版本中
    QString pluginPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (Utils::HostOsInfo::isAnyUnixHost() && !Utils::HostOsInfo::isMacHost())
        pluginPath += QLatin1String("/data");
    pluginPath += QLatin1Char('/')
            + QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR)
            + QLatin1Char('/');
    pluginPath += QLatin1String(Core::Constants::IDE_ID);
    pluginPath += QLatin1String("/plugins/");
    // Qt创建者X.Y.Z可以从x . y (Z-1)等加载插件，所以添加当前和以前补丁版本

    const QString minorVersion = QString::number(IDE_VERSION_MAJOR) + '.'+ QString::number(IDE_VERSION_MINOR) + '.';
    const int minPatchVersion= qMin(IDE_VERSION_RELEASE,
               QVersionNumber::fromString(Core::Constants::IDE_VERSION_COMPAT).microVersion());
    for (int patchVersion = IDE_VERSION_RELEASE; patchVersion >= minPatchVersion; --patchVersion)
        rc.push_back(pluginPath + minorVersion + QString::number(patchVersion));
    return rc;
}

//注册安装设置
static void setupInstallSettings(QString &installSettingspath)
{
    if (!installSettingspath.isEmpty() && !QFileInfo(installSettingspath).isDir()) {
        displayError(QString("-installsettingspath \"%0\" needs to be the path where a %1/%2.ini exist.").arg(installSettingspath,
            QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR), QLatin1String(Core::Constants::IDE_CASED_ID)));
        installSettingspath.clear();
    }
//检查默认安装设置是否包含实际安装设置。//这可以是绝对路径，也可以是相对于applicationDirPath()的路径。
    //结果的解释类似于-settingspath，但对于SystemScope
    static const char kInstallSettingsKey[] = "Settings/InstallSettings";
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
        installSettingspath.isEmpty() ? resourcePath() : installSettingspath);

    QSettings installSettings(QSettings::IniFormat, QSettings::UserScope,
                              QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR),
                              QLatin1String(Core::Constants::IDE_CASED_ID));
    if (installSettings.contains(kInstallSettingsKey)) {
        QString installSettingsPath = installSettings.value(kInstallSettingsKey).toString();
        if (QDir::isRelativePath(installSettingsPath))
            installSettingsPath = applicationDirPath() + '/' + installSettingsPath;
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, installSettingsPath);
    }
}

static QSettings *createUserSettings()
{
    return new QSettings(QSettings::IniFormat, QSettings::UserScope,
                         QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR),
                         QLatin1String(Core::Constants::IDE_CASED_ID));
}

static inline QSettings *userSettings()
{
    QSettings *settings = createUserSettings();
    const QString fromVariant = QLatin1String(Core::Constants::IDE_COPY_SETTINGS_FROM_VARIANT_STR);
    if (fromVariant.isEmpty())
        return settings;

    // Copy old settings to new ones:
    QFileInfo pathFi = QFileInfo(settings->fileName());
    if (pathFi.exists()) // already copied.
        return settings;

    QDir destDir = pathFi.absolutePath();
    if (!destDir.exists())
        destDir.mkpath(pathFi.absolutePath());

    QDir srcDir = destDir;
    srcDir.cdUp();
    if (!srcDir.cd(fromVariant))
        return settings;

    if (srcDir == destDir) // Nothing to copy and no settings yet
        return settings;

    const QStringList entries = srcDir.entryList();
    for (const QString &file : entries) {
        const QString lowerFile = file.toLower();
        if (lowerFile.startsWith(QLatin1String("profiles.xml"))
                || lowerFile.startsWith(QLatin1String("toolchains.xml"))
                || lowerFile.startsWith(QLatin1String("qtversion.xml"))
                || lowerFile.startsWith(QLatin1String("devices.xml"))
                || lowerFile.startsWith(QLatin1String("debuggers.xml"))
                || lowerFile.startsWith(QLatin1String(Core::Constants::IDE_ID) + "."))
            QFile::copy(srcDir.absoluteFilePath(file), destDir.absoluteFilePath(file));
        if (file == QLatin1String(Core::Constants::IDE_ID))
            copyRecursively(srcDir.absoluteFilePath(file), destDir.absoluteFilePath(file));
    }

    // Make sure to use the copied settings:
    delete settings;
    return createUserSettings();
}

//设置高Dpi环境变量
static void setHighDpiEnvironmentVariable()
{

    if (Utils::HostOsInfo().isMacHost())
        return;

    std::unique_ptr<QSettings> settings(createUserSettings());

    const bool defaultValue = Utils::HostOsInfo().isWindowsHost();
    const bool enableHighDpiScaling = settings->value("Core/EnableHighDpiScaling", defaultValue).toBool();

    static const char ENV_VAR_QT_DEVICE_PIXEL_RATIO[] = "QT_DEVICE_PIXEL_RATIO";
    if (enableHighDpiScaling
            && !qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO) // legacy in 5.6, but still functional
            && !qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION == QT_VERSION_CHECK(5, 14, 0)
        // work around QTBUG-80934
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::Round);
#endif
    }
}

void loadFonts()
{
    const QDir dir(resourcePath() + "/fonts/");

    const QFileInfoList fonts = dir.entryInfoList(QStringList("*.ttf"), QDir::Files);
    for (const QFileInfo &fileInfo : fonts)
        QFontDatabase::addApplicationFont(fileInfo.absoluteFilePath());
}

///选项
struct Options
{
    QString settingsPath;
    QString installSettingsPath;
    QStringList customPluginPaths;
    //已处理且未传递给应用程序或插件管理器的参数列表
    QStringList preAppArguments;
    //要传递给应用程序或插件管理器的参数列表
    std::vector<char *> appArguments;
    Utils::optional<QString> userLibraryPath;
    bool hasTestOption = false;
    bool wantsCleanSettings = false;
};

///命令行参数转化为Options结构体数据
Options parseCommandLine(int argc, char *argv[])
{

    for(int x=0;x<argc;x++){

        qDebug()<<argc<<"-----"<<argv[x]<<endl;
    }
    Options options;
    auto it = argv;
    const auto end = argv + argc;
    while (it != end) {
        const auto arg = QString::fromLocal8Bit(*it);
        const bool hasNext = it + 1 != end;
        const auto nextArg = hasNext ? QString::fromLocal8Bit(*(it + 1)) : QString();

        if (arg == SETTINGS_OPTION && hasNext) {
            ++it;
            options.settingsPath = QDir::fromNativeSeparators(nextArg);
            options.preAppArguments << arg << nextArg;
        } else if (arg == INSTALL_SETTINGS_OPTION && hasNext) {
            ++it;
            options.installSettingsPath = QDir::fromNativeSeparators(nextArg);
            options.preAppArguments << arg << nextArg;
        } else if (arg == PLUGINPATH_OPTION && hasNext) {
            ++it;
            options.customPluginPaths += QDir::fromNativeSeparators(nextArg);
            options.preAppArguments << arg << nextArg;
        } else if (arg == USER_LIBRARY_PATH_OPTION && hasNext) {
            ++it;
            options.userLibraryPath = nextArg;
            options.preAppArguments << arg << nextArg;
        } else if (arg == TEMPORARY_CLEAN_SETTINGS1 || arg == TEMPORARY_CLEAN_SETTINGS2) {
            options.wantsCleanSettings = true;
            options.preAppArguments << arg;
        } else { // arguments that are still passed on to the application
            if (arg == TEST_OPTION)
                options.hasTestOption = true;
            options.appArguments.push_back(*it);
        }
        ++it;
    }
    return options;
}


///自动重启类
class Restarter
{
public:
    Restarter(int argc, char *argv[])
    {
        Q_UNUSED(argc)
        m_executable = QString::fromLocal8Bit(argv[0]);
        m_workingPath = QDir::currentPath();
    }

    void setArguments(const QStringList &args) { m_args = args; }

    QStringList arguments() const { return m_args; }

    int restartOrExit(int exitCode)
    {
        return qApp->property("restart").toBool() ? restart(exitCode) : exitCode;
    }

    int restart(int exitCode)
    {
        QProcess::startDetached(m_executable, m_args, m_workingPath);
        return exitCode;
    }

private:
    QString m_executable;
    QStringList m_args;
    QString m_workingPath;
};

QStringList lastSessionArgument()
{
    // 无论如何，在这里使用内幕信息并不是特别好
    const bool hasProjectExplorer = Utils::anyOf(PluginManager::plugins(),
                                                 Utils::equal(&PluginSpec::name,
                                                              QString("ProjectExplorer")));
    return hasProjectExplorer ? QStringList({"-lastsession"}) : QStringList();
}

int main(int argc, char **argv)
{
    Restarter restarter(argc, argv);
    Utils::Environment::systemEnvironment(); // 在我们做任何更改之前缓存系统环境
    //手动格式化各种命令行选项
    //我们不能使用常规的插件管理器，
    //因为设置可以改变插件管理器的行为方式
    Options options = parseCommandLine(argc, argv);
    applicationDirPath(argv[0]);

    if (qEnvironmentVariableIsSet("QTC_DO_NOT_PROPAGATE_LD_PRELOAD")) {
        Utils::Environment::modifySystemEnvironment(
            {{"LD_PRELOAD", "", Utils::EnvironmentItem::Unset}});//不设置环境项目
    }

    if (options.userLibraryPath) {
        if ((*options.userLibraryPath).isEmpty()) {
            Utils::Environment::modifySystemEnvironment(
                {{"LD_LIBRARY_PATH", "", Utils::EnvironmentItem::Unset}});
        } else {
            Utils::Environment::modifySystemEnvironment(
                {{"LD_LIBRARY_PATH", *options.userLibraryPath, Utils::EnvironmentItem::SetEnabled}});
        }
    }

#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("QT_OPENGL"))
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
#endif

    if (qEnvironmentVariableIsSet("QTCREATOR_DISABLE_NATIVE_MENUBAR")
            || qgetenv("XDG_CURRENT_DESKTOP").startsWith("Unity")) {
        QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    }

    Utils::TemporaryDirectory::setMasterTemporaryDirectory(QDir::tempPath() + "/" + Core::Constants::IDE_CASED_ID + "-XXXXXX");

    QScopedPointer<Utils::TemporaryDirectory> temporaryCleanSettingsDir;
    if (options.settingsPath.isEmpty() && (options.hasTestOption || options.wantsCleanSettings)) {
        temporaryCleanSettingsDir.reset(new Utils::TemporaryDirectory("qtc-test-settings"));
        if (!temporaryCleanSettingsDir->isValid())
            return 1;
        options.settingsPath = temporaryCleanSettingsDir->path();
    }
    if (!options.settingsPath.isEmpty())
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, options.settingsPath);

    // 必须在创建任何QSettings类之前完成
    QSettings::setDefaultFormat(QSettings::IniFormat);
    setupInstallSettings(options.installSettingsPath);
    //插件管理器控制这个设置对象

    setHighDpiEnvironmentVariable();

    SharedTools::QtSingleApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    int numberofArguments = static_cast<int>(options.appArguments.size());

    SharedTools::QtSingleApplication app((QLatin1String(Core::Constants::IDE_DISPLAY_NAME)),
                                         numberofArguments,
                                         options.appArguments.data());
    QCoreApplication::setApplicationName(Core::Constants::IDE_CASED_ID);
    QCoreApplication::setApplicationVersion(QLatin1String(Core::Constants::IDE_VERSION_LONG));
    QCoreApplication::setOrganizationName(QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR));
    QGuiApplication::setApplicationDisplayName(Core::Constants::IDE_DISPLAY_NAME);

    const QStringList pluginArguments = app.arguments();

    /*初始化全局设置并使用QApplication::applicationDirPath重新设置安装设置 */
    setupInstallSettings(options.installSettingsPath);
    QSettings *settings = userSettings();
    QSettings *globalSettings = new QSettings(QSettings::IniFormat, QSettings::SystemScope,
                                              QLatin1String(Core::Constants::IDE_SETTINGSVARIANT_STR),
                                              QLatin1String(Core::Constants::IDE_CASED_ID));
    loadFonts();

    if (Utils::HostOsInfo().isWindowsHost() && !qFuzzyCompare(qApp->devicePixelRatio(), 1.0)
            && QApplication::style()->objectName().startsWith(
                QLatin1String("windows"), Qt::CaseInsensitive)) {
        QApplication::setStyle(QLatin1String("fusion"));
    }
    const int threadCount = QThreadPool::globalInstance()->maxThreadCount();
    QThreadPool::globalInstance()->setMaxThreadCount(qMax(4, 2 * threadCount));

    const QString libexecPath = QCoreApplication::applicationDirPath()
            + '/' + RELATIVE_LIBEXEC_PATH;

#ifdef ENABLE_QT_BREAKPAD
    QtSystemExceptionHandler systemExceptionHandler(libexecPath);
#else
    // 一旦发送了一个严重的信号，就显示一个回溯(仅适用于Linux)。
    CrashHandlerSetup setupCrashHandler(Core::Constants::IDE_DISPLAY_NAME, CrashHandlerSetup::EnableRestart, libexecPath);

#endif

    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    app.setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

    PluginManager pluginManager;
    PluginManager::setPluginIID(QLatin1String("org.qt-project.Qt.QtCreatorPlugin"));
    PluginManager::setGlobalSettings(globalSettings);
    PluginManager::setSettings(settings);

    QTranslator translator;
    QTranslator qtTranslator;
    QStringList uiLanguages = QLocale::system().uiLanguages();
    QString overrideLanguage = settings->value(QLatin1String("General/OverrideLanguage")).toString();
    if (!overrideLanguage.isEmpty())
        uiLanguages.prepend(overrideLanguage);
    const QString &creatorTrPath = resourcePath() + "/translations";
    for (QString locale : qAsConst(uiLanguages)) {
        locale = QLocale(locale).name();
        if (translator.load("qtcreator_" + locale, creatorTrPath)) {
            const QString &qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
            const QString &qtTrFile = QLatin1String("qt_") + locale;
            // 二进制安装程序将Qt tr文件放入creatorTrPath中
            if (qtTranslator.load(qtTrFile, qtTrPath) || qtTranslator.load(qtTrFile, creatorTrPath)) {
                app.installTranslator(&translator);
                app.installTranslator(&qtTranslator);
                app.setProperty("qtc_locale", locale);
                break;
            }
            translator.load(QString()); // 卸载()
        } else if (locale == QLatin1String("C") /* 覆盖语言 == "English" */) {
            // use built-in
            break;
        } else if (locale.startsWith(QLatin1String("en")) /* "English" is built-in */) {
            // use built-in
            break;
        }
    }

    app.setDesktopFileName("org.qt-project.qtcreator.desktop");

    // 确保我们尊重系统的代理设置
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // 加载插件
    const QStringList pluginPaths = getPluginPaths() + options.customPluginPaths;
    PluginManager::setPluginPaths(pluginPaths);
    QMap<QString, QString> foundAppOptions;
    if (pluginArguments.size() > 1) {
        QMap<QString, bool> appOptions;
        appOptions.insert(QLatin1String(HELP_OPTION1), false);
        appOptions.insert(QLatin1String(HELP_OPTION2), false);
        appOptions.insert(QLatin1String(HELP_OPTION3), false);
        appOptions.insert(QLatin1String(HELP_OPTION4), false);
        appOptions.insert(QLatin1String(VERSION_OPTION), false);
        appOptions.insert(QLatin1String(CLIENT_OPTION), false);
        appOptions.insert(QLatin1String(PID_OPTION), true);
        appOptions.insert(QLatin1String(BLOCK_OPTION), false);
        QString errorMessage;
        if (!PluginManager::parseOptions(pluginArguments, appOptions, &foundAppOptions, &errorMessage)) {
            displayError(errorMessage);
            printHelp(QFileInfo(app.applicationFilePath()).baseName());
            return -1;
        }
    }
    restarter.setArguments(options.preAppArguments + PluginManager::argumentsForRestart()
                           + lastSessionArgument());

//在插件列表里面寻找core插件找到之后,将插件信息拷贝给coreplugin并直接退出
    const PluginSpecSet plugins = PluginManager::plugins();
    PluginSpec *coreplugin = nullptr;
    for (PluginSpec *spec : plugins) {
        if (spec->name() == QLatin1String(corePluginNameC)) {
            coreplugin = spec;
            break;
        }
    }
//检测coreplugin插件是否存在
    if (!coreplugin) {
        QString nativePaths = QDir::toNativeSeparators(pluginPaths.join(QLatin1Char(',')));
        const QString reason = QCoreApplication::translate("Application", "不能在%1找到core插件").arg(nativePaths);
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
//检测coreplugin插件是否开启
    if (!coreplugin->isEffectivelyEnabled()) {
        const QString reason = QCoreApplication::translate("Application", "core插件被禁用.");
        displayError(msgCoreLoadFailure(reason));
        return 1;
    }
//检测coreplugin插件是否发生错误
    if (coreplugin->hasError()) {
        displayError(msgCoreLoadFailure(coreplugin->errorString()));
        return 1;
    }
//如果插件选项里面包含版本选项则打印
    if (foundAppOptions.contains(QLatin1String(VERSION_OPTION))) {
        printVersion(coreplugin);
        return 0;
    }
//如果插件选项里面包含help相关子则打印帮助信息
    if (foundAppOptions.contains(QLatin1String(HELP_OPTION1))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION2))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION3))
            || foundAppOptions.contains(QLatin1String(HELP_OPTION4))) {
        printHelp(QFileInfo(app.applicationFilePath()).baseName());
        return 0;
    }

//保存插件的进程
    qint64 pid = -1;
    if (foundAppOptions.contains(QLatin1String(PID_OPTION))) {
        QString pidString = foundAppOptions.value(QLatin1String(PID_OPTION));
        bool pidOk;
        qint64 tmpPid = pidString.toInt(&pidOk);
        if (pidOk)
            pid = tmpPid;
    }

    bool isBlock = foundAppOptions.contains(QLatin1String(BLOCK_OPTION));
    if (app.isRunning() && (pid != -1 || isBlock || foundAppOptions.contains(QLatin1String(CLIENT_OPTION)))) {
        app.setBlock(isBlock);
        if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
            return 0;

        // 信息无法发送，可能是在退出的过程中
        if (app.isRunning(pid)) {
            // 应用程序还在运行，询问用户
            int button = askMsgSendFailed();
            while (button == QMessageBox::Retry) {
                if (app.sendMessage(PluginManager::serializedArguments(), 5000 /*timeout*/, pid))
                    return 0;
                if (!app.isRunning(pid)) //应用程序退出时，我们试图开始一个新的
                    button = QMessageBox::Yes;
                else
                    button = askMsgSendFailed();
            }
            if (button == QMessageBox::No)
                return -1;
        }
    }
//检查有问题的插件
    PluginManager::checkForProblematicPlugins();
//加载coreplugin插件
    PluginManager::loadPlugins();
    if (coreplugin->hasError()) {
        displayError(msgCoreLoadFailure(coreplugin->errorString()));
        return 1;
    }

    // 设置当消息成功到达的时候移除插件管理器的参数
    QObject::connect(&app, &SharedTools::QtSingleApplication::messageReceived,
                     &pluginManager, &PluginManager::remoteArguments);

    QObject::connect(&app, SIGNAL(fileOpenRequest(QString)), coreplugin->plugin(),
                     SLOT(fileOpenRequest(QString)));

    // 退出时关闭插件管理器
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &pluginManager, &PluginManager::shutdown);

    return restarter.restartOrExit(app.exec());
}
