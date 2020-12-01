#pragma once

namespace Core {
namespace Constants {

#define STRINGIFY_INTERNAL(x) #x
#define STRINGIFY(x) STRINGIFY_INTERNAL(x)

const char IDE_DISPLAY_NAME[] = "Qt Creator";
const char IDE_ID[] = "qtcreator";
const char IDE_CASED_ID[] = "QtCreator";

#define IDE_VERSION 4.13.82
#define IDE_VERSION_STR STRINGIFY(IDE_VERSION)
#define IDE_VERSION_DISPLAY_DEF 4.14.0-beta1
#define IDE_VERSION_COMPAT_DEF 4.13.82

#define IDE_VERSION_MAJOR 4
#define IDE_VERSION_MINOR 13
#define IDE_VERSION_RELEASE 82

const char IDE_VERSION_LONG[]      = IDE_VERSION_STR;
const char IDE_VERSION_DISPLAY[]   = STRINGIFY(IDE_VERSION_DISPLAY_DEF);
const char IDE_VERSION_COMPAT[]    = STRINGIFY(IDE_VERSION_COMPAT_DEF);
const char IDE_AUTHOR[]            = "The Qt Company Ltd";
const char IDE_YEAR[]              = "2020";

#ifdef IDE_REVISION
const char IDE_REVISION_STR[]      = STRINGIFY(IDE_REVISION);
#else
const char IDE_REVISION_STR[]      = "";
#endif

const char IDE_REVISION_URL[]  = "";

// changes the path where the settings are saved to
#ifdef IDE_SETTINGSVARIANT
const char IDE_SETTINGSVARIANT_STR[]      = STRINGIFY(IDE_SETTINGSVARIANT);
#else
const char IDE_SETTINGSVARIANT_STR[]      = "QtProject";
#endif

#ifdef IDE_COPY_SETTINGS_FROM_VARIANT
const char IDE_COPY_SETTINGS_FROM_VARIANT_STR[] = STRINGIFY(IDE_COPY_SETTINGS_FROM_VARIANT);
#else
const char IDE_COPY_SETTINGS_FROM_VARIANT_STR[] = "";
#endif

#undef IDE_VERSION_COMPAT_DEF
#undef IDE_VERSION_DISPLAY_DEF
#undef IDE_VERSION
#undef IDE_VERSION_STR
#undef STRINGIFY
#undef STRINGIFY_INTERNAL

} // Constants
} // Core
