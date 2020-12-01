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

#include "../utils_global.h"

#include <QSharedPointer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QRect>
#include <QVariant>

/*!
当前形式的QToolTip是不可扩展的。因此，这是为Creator提供一种更加灵活和可定制的工具提示机制的尝试。
 这里的部分代码是来自QToolTip的复制。这包括一个私有的Qt头和一个未导出的类QTipLabel，这里用作一个基本的tip类。
请注意Qt依赖于这个特定的类名，以便正确地应用工具提示的本地样式。因此QTipLabel名称不应该被改变。
 */

QT_BEGIN_NAMESPACE
class QPoint;
class QVariant;
class QLayout;
class QWidget;
QT_END_NAMESPACE

namespace Utils {
namespace Internal { class TipLabel; }

class QTCREATOR_UTILS_EXPORT ToolTip : public QObject
{
    Q_OBJECT
protected:
    ToolTip();

public:
    ~ToolTip() override;

    enum {
        ColorContent = 0,
        TextContent = 1,
        WidgetContent = 42
    };

    bool eventFilter(QObject *o, QEvent *event) override;

    static ToolTip *instance();

    static void show(const QPoint &pos, const QString &content, QWidget *w = nullptr,
                     const QVariant &contextHelp = {}, const QRect &rect = QRect());
    static void show(const QPoint &pos,
                     const QString &content,
                     Qt::TextFormat format,
                     QWidget *w = nullptr,
                     const QVariant &contextHelp = {},
                     const QRect &rect = QRect());
    static void show(const QPoint &pos,
                     const QColor &color,
                     QWidget *w = nullptr,
                     const QVariant &contextHelp = {},
                     const QRect &rect = QRect());
    static void show(const QPoint &pos, QWidget *content, QWidget *w = nullptr,
                     const QVariant &contextHelp = {}, const QRect &rect = QRect());
    static void show(const QPoint &pos, QLayout *content, QWidget *w = nullptr,
                     const QVariant &contextHelp = {}, const QRect &rect = QRect());
    static void move(const QPoint &pos);
    static void hide();
    static void hideImmediately();
    static bool isVisible();

    static QPoint offsetFromPosition();

    ///帮助“pin”(显示为真实窗口)工具提示显示使用WidgetContent
    static bool pinToolTip(QWidget *w, QWidget *parent);

    static QVariant contextHelp();

signals:
    void shown();
    void hidden();

private:
    void showInternal(const QPoint &pos, const QVariant &content, int typeId, QWidget *w,
                      const QVariant &contextHelp, const QRect &rect);
    void hideTipImmediately();
    bool acceptShow(const QVariant &content, int typeId, const QPoint &pos, QWidget *w,
                    const QVariant &contextHelp, const QRect &rect);
    void setUp(const QPoint &pos, QWidget *w, const QRect &rect);
    bool tipChanged(const QPoint &pos, const QVariant &content, int typeId, QWidget *w,
                    const QVariant &contextHelp) const;
    void setTipRect(QWidget *w, const QRect &rect);
    void placeTip(const QPoint &pos);
    void showTip();
    void hideTipWithDelay();

    QPointer<Internal::TipLabel> m_tip;
    QWidget *m_widget;
    QRect m_rect;
    QTimer m_showTimer;
    QTimer m_hideDelayTimer;
    QVariant m_contextHelp;
};

} // namespace Utils
