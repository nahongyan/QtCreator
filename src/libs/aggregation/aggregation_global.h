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
//保证头文件仅被包含一次
#pragma once

#include <qglobal.h>

//下面的语句定义了AGGREGATION_LIBRARY宏，用于动态库的导出导入。
//由于我们在 aggregation.pro 中定义了宏AGGREGATION_LIBRARY，因此，AGGREGATION_LIBRARY将被定义为Q_DECL_EXPORT。
//这是 Qt 库项目的标准写法，每次使用 Qt Creator 生成代码时总会有类似的形式
#if defined(AGGREGATION_LIBRARY)
#  define AGGREGATION_EXPORT Q_DECL_EXPORT
#else
#  define AGGREGATION_EXPORT Q_DECL_IMPORT
#endif
