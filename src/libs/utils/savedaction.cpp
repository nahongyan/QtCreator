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

#include <utils/savedaction.h>

#include <utils/qtcassert.h>
#include <utils/pathchooser.h>
#include <utils/pathlisteditor.h>

#include <QActionGroup>
#include <QCheckBox>
#include <QDebug>
#include <QGroupBox>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>

namespace Utils {

//////////////////////////////////////////////////////////////////////////
//
// SavedAction
//
//////////////////////////////////////////////////////////////////////////

/*!
    \class Utils::SavedAction

    \brief SavedAction类是持久化操作的助手类状态。

*/

SavedAction::SavedAction(QObject *parent)
{
    setParent(parent);
    connect(&m_action, &QAction::triggered, this, &SavedAction::actionTriggered);
}


/*!
    返回对象的当前值。

    \sa setValue()
*/
QVariant SavedAction::value() const
{
    return m_value;
}


/*!
设置对象的当前值。如果值发生了变化,doemit为真，则valueChanged()信号将被发射。


    \sa value()
*/
void SavedAction::setValue(const QVariant &value, bool doemit)
{
    if (value == m_value)
        return;
    m_value = value;
    if (m_action.isCheckable())
        m_action.setChecked(m_value.toBool());
    if (doemit)
        emit valueChanged(m_value);
}


/*!
返回项目尚不存在时使用的默认值在设置。


    \sa setDefaultValue()
*/
QVariant SavedAction::defaultValue() const
{
    return m_defaultValue;
}


/*!
设置项目尚不存在时使用的默认值在设置。


    \sa defaultValue()
*/
void SavedAction::setDefaultValue(const QVariant &value)
{
    m_defaultValue = value;
}


QString SavedAction::toString() const
{
    return QLatin1String("value: ") + m_value.toString()
        + QLatin1String("  defaultvalue: ") + m_defaultValue.toString()
        + QLatin1String("  settingskey: ") + m_settingsKey;
}

/*
    Uses \c settingsGroup() and \c settingsKey() to restore the
    item from \a settings,

    \sa settingsKey(), settingsGroup(), writeSettings()
*/
void SavedAction::readSettings(const QSettings *settings)
{
    if (m_settingsKey.isEmpty())
        return;
    QVariant var = settings->value(m_settingsKey, m_defaultValue);
    // work around old ini files containing @Invalid() entries
    if (m_action.isCheckable() && !var.isValid())
        var = false;
    setValue(var);
}

/*
    Uses \c settingsGroup() and \c settingsKey() to write the
    item to \a settings,

    \sa settingsKey(), settingsGroup(), readSettings()
*/
void SavedAction::writeSettings(QSettings *settings)
{
    if (m_settingsKey.isEmpty())
        return;
    settings->setValue(m_settingsKey, m_value);
}

/*!
一个 SavedAction可以连接到一个小部件，通常是一个
某些配置对话框中的复选框、radiobutton或lineedit。
小部件将从SavedAction检索其内容值，并且——取决于一个ApplyMode——要么写
立即返回更改，或当 SavedAction::apply()被称为明确。
    \sa apply(), disconnectWidget()
*/
void SavedAction::connectWidget(QWidget *widget, ApplyMode applyMode)
{
    QTC_ASSERT(!m_widget,
        qDebug() << "ALREADY CONNECTED: " << widget << m_widget << toString(); return);
    m_widget = widget;

    if (auto button = qobject_cast<QCheckBox *>(widget)) {
        if (!m_dialogText.isEmpty())
            button->setText(m_dialogText);
        button->setChecked(m_value.toBool());
        if (applyMode == ImmediateApply) {
            connect(button, &QCheckBox::clicked,
                    this, [this, button] { setValue(button->isChecked()); });
        }
    } else if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
        spinBox->setValue(m_value.toInt());
        if (applyMode == ImmediateApply) {
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [this, spinBox]() { setValue(spinBox->value()); });
        }
    } else if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
        lineEdit->setText(m_value.toString());
        if (applyMode == ImmediateApply) {
            connect(lineEdit, &QLineEdit::editingFinished,
                    this, [this, lineEdit] { setValue(lineEdit->text()); });
        }

    } else if (auto pathChooser = qobject_cast<PathChooser *>(widget)) {
        pathChooser->setPath(m_value.toString());
        if (applyMode == ImmediateApply) {
            auto finished = [this, pathChooser] { setValue(pathChooser->path()); };
            connect(pathChooser, &PathChooser::editingFinished, this, finished);
            connect(pathChooser, &PathChooser::browsingFinished, this, finished);
        }
    } else if (auto groupBox = qobject_cast<QGroupBox *>(widget)) {
        if (!groupBox->isCheckable())
            qDebug() << "连接到不可选组框" << widget << toString();
        groupBox->setChecked(m_value.toBool());
        if (applyMode == ImmediateApply) {
            connect(groupBox, &QGroupBox::toggled,
                    this, [this, groupBox] { setValue(QVariant(groupBox->isChecked())); });
        }
    } else if (auto textEdit = qobject_cast<QTextEdit *>(widget)) {
        textEdit->setPlainText(m_value.toString());
        if (applyMode == ImmediateApply) {
            connect(textEdit, &QTextEdit::textChanged,
                    this, [this, textEdit] { setValue(textEdit->toPlainText()); });
        }
    } else if (auto editor = qobject_cast<PathListEditor *>(widget)) {
        editor->setPathList(m_value.toStringList());
    } else {
        qDebug() << "不能连接部件" << widget << toString();
    }

    // 复制工具提示，但只有在小部件上还没有明确设置的情况下。
    if (widget->toolTip().isEmpty())
        widget->setToolTip(m_action.toolTip());
}

/*
    从小部件断开 SavedAction。

    \sa apply(), connectWidget()
*/
void SavedAction::disconnectWidget()
{
    m_widget = nullptr;
}

void SavedAction::apply(QSettings *s)
{
    if (auto button = qobject_cast<QCheckBox *>(m_widget))
        setValue(button->isChecked());
    else if (auto lineEdit = qobject_cast<QLineEdit *>(m_widget))
        setValue(lineEdit->text());
    else if (auto spinBox = qobject_cast<QSpinBox *>(m_widget))
        setValue(spinBox->value());
    else if (auto pathChooser = qobject_cast<PathChooser *>(m_widget))
        setValue(pathChooser->path());
    else if (auto groupBox = qobject_cast<QGroupBox *>(m_widget))
        setValue(groupBox->isChecked());
    else if (auto textEdit = qobject_cast<QTextEdit *>(m_widget))
        setValue(textEdit->toPlainText());
    else if (auto editor = qobject_cast<PathListEditor *>(m_widget))
        setValue(editor->pathList());
    if (s)
       writeSettings(s);
}

/*
如果此SavedAction是，则标签中使用的默认文本在设置对话框中使用。
这通常与SavedAction显示的文本类似在菜单中使用，但大小写不同。
    \sa text()
*/
QString SavedAction::dialogText() const
{
    return m_dialogText;
}

void SavedAction::setDialogText(const QString &dialogText)
{
    m_dialogText = dialogText;
}

void SavedAction::actionTriggered(bool)
{
    if (m_action.isCheckable())
        setValue(m_action.isChecked());
    if (m_action.actionGroup() && m_action.actionGroup()->isExclusive()) {
        // FIXME: 应该更直接地处理吗
        const QList<QAction *> actions = m_action.actionGroup()->actions();
        for (QAction *act : actions)
            if (auto dact = qobject_cast<SavedAction *>(act))
                dact->setValue(bool(act == &m_action));
    }
}

QAction *SavedAction::action()
{
    return &m_action;
}

void SavedAction::trigger(const QVariant &data)
{
    m_action.setData(data);
    m_action.trigger();
}

//////////////////////////////////////////////////////////////////////////
//
// SavedActionSet
//
//////////////////////////////////////////////////////////////////////////

void SavedActionSet::insert(SavedAction *action, QWidget *widget)
{
    m_list.append(action);
    if (widget)
        action->connectWidget(widget);
}

void SavedActionSet::apply(QSettings *settings)
{
    for (SavedAction *action : qAsConst(m_list))
        action->apply(settings);
}

void SavedActionSet::finish()
{
    for (SavedAction *action : qAsConst(m_list))
        action->disconnectWidget();
}

} // namespace Utils
