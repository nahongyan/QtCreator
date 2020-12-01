/****************************************************************************
**
** Copyright (C) 2016 Petar Perisin <petar.perisin@gmail.com>
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

#include "ansiescapecodehandler.h"
#include <utils/qtcassert.h>

namespace Utils {

/*!
    \class Utils::AnsiEscapeCodeHandler

    \brief AnsiEscapeCodeHandler类解析文本并从中提取ANSI转义码。为了跨文本段保存颜色信息，必须在流的生命周期内存储该类的实例。
此外，该类的一个实例不应该处理多个流(至少不能同时处理)。它的主要函数是parseText()，它接受文本和默认的QTextCharFormat。
这个函数用于解析文本并将彩色文本拆分为较小的字符串，在QTextCharFormat中设置适当的格式信息。

    使用:
    \list
    \li 为流创建AnsiEscapeCodeHandler的新实例。
    \li 要添加新文本，使用文本和默认的QTextCharFormat调用parseText()。这个函数的结果是设置了适当格式的字符串列表QTextCharFormat。
    \endlist
*/

static QColor ansiColor(uint code)
{
    QTC_ASSERT(code < 8, return QColor());

    const int red   = code & 1 ? 170 : 0;
    const int green = code & 2 ? 170 : 0;
    const int blue  = code & 4 ? 170 : 0;
    return QColor(red, green, blue);
}

QList<FormattedText> AnsiEscapeCodeHandler::parseText(const FormattedText &input)
{
    enum AnsiEscapeCodes {
        ResetFormat            =  0,
        BoldText               =  1,
        TextColorStart         = 30,
        TextColorEnd           = 37,
        RgbTextColor           = 38,
        DefaultTextColor       = 39,
        BackgroundColorStart   = 40,
        BackgroundColorEnd     = 47,
        RgbBackgroundColor     = 48,
        DefaultBackgroundColor = 49
    };

    const QString escape        = "\x1b[";
    const QChar semicolon       = ';';
    const QChar colorTerminator = 'm';
    const QChar eraseToEol      = 'K';

    QList<FormattedText> outputData;
    QTextCharFormat charFormat = m_previousFormatClosed ? input.format : m_previousFormat;
    QString strippedText;
    if (m_pendingText.isEmpty()) {
        strippedText = input.text;
    } else {
        strippedText = m_pendingText.append(input.text);
        m_pendingText.clear();
    }

    while (!strippedText.isEmpty()) {
        QTC_ASSERT(m_pendingText.isEmpty(), break);
        if (m_waitingForTerminator) {
            // 我们忽略所有接受字符串参数的转义代码。
            QString terminator = "\x1b\\";
            int terminatorPos = strippedText.indexOf(terminator);
            if (terminatorPos == -1 && !m_alternateTerminator.isEmpty()) {
                terminator = m_alternateTerminator;
                terminatorPos = strippedText.indexOf(terminator);
            }
            if (terminatorPos == -1) {
                m_pendingText = strippedText;
                break;
            }
            m_waitingForTerminator = false;
            m_alternateTerminator.clear();
            strippedText.remove(0, terminatorPos + terminator.length());
            if (strippedText.isEmpty())
                break;
        }
        const int escapePos = strippedText.indexOf(escape.at(0));
        if (escapePos < 0) {
            outputData << FormattedText(strippedText, charFormat);
            break;
        } else if (escapePos != 0) {
            outputData << FormattedText(strippedText.left(escapePos), charFormat);
            strippedText.remove(0, escapePos);
        }
        QTC_ASSERT(strippedText.at(0) == escape.at(0), break);

        while (!strippedText.isEmpty() && escape.at(0) == strippedText.at(0)) {
            if (escape.startsWith(strippedText)) {
                //控制程序不完整
                m_pendingText += strippedText;
                strippedText.clear();
                break;
            }
            if (!strippedText.startsWith(escape)) {
                switch (strippedText.at(1).toLatin1()) {
                case '\\': // 想不到的终结者序列。
                    QTC_CHECK(false);
                    Q_FALLTHROUGH();
                case 'N': case 'O': //忽略不支持的单字符序列
                    strippedText.remove(0, 2);
                    break;
                case ']':
                    m_alternateTerminator = QChar(7);
                    Q_FALLTHROUGH();
                case 'P':  case 'X': case '^': case '_':
                    strippedText.remove(0, 2);
                    m_waitingForTerminator = true;
                    break;
                default:
                    // 不是控制序列
                    m_pendingText.clear();
                    outputData << FormattedText(strippedText.left(1), charFormat);
                    strippedText.remove(0, 1);
                    continue;
                }
                break;
            }
            m_pendingText += strippedText.mid(0, escape.length());
            strippedText.remove(0, escape.length());

            // \e[K不支持。只是带.
            if (strippedText.startsWith(eraseToEol)) {
                m_pendingText.clear();
                strippedText.remove(0, 1);
                continue;
            }
            // 得到的数字
            QString strNumber;
            QStringList numbers;
            while (!strippedText.isEmpty()) {
                if (strippedText.at(0).isDigit()) {
                    strNumber += strippedText.at(0);
                } else {
                    if (!strNumber.isEmpty())
                        numbers << strNumber;
                    if (strNumber.isEmpty() || strippedText.at(0) != semicolon)
                        break;
                    strNumber.clear();
                }
                m_pendingText += strippedText.mid(0, 1);
                strippedText.remove(0, 1);
            }
            if (strippedText.isEmpty())
                break;

            // 删除终止字符
            if (!strippedText.startsWith(colorTerminator)) {
                m_pendingText.clear();
                strippedText.remove(0, 1);
                break;
            }
            // 获得一致的控制序列，可以清除挂起的文本
            m_pendingText.clear();
            strippedText.remove(0, 1);

            if (numbers.isEmpty()) {
                charFormat = input.format;
                endFormatScope();
            }

            for (int i = 0; i < numbers.size(); ++i) {
                const uint code = numbers.at(i).toUInt();

                if (code >= TextColorStart && code <= TextColorEnd) {
                    charFormat.setForeground(ansiColor(code - TextColorStart));
                    setFormatScope(charFormat);
                } else if (code >= BackgroundColorStart && code <= BackgroundColorEnd) {
                    charFormat.setBackground(ansiColor(code - BackgroundColorStart));
                    setFormatScope(charFormat);
                } else {
                    switch (code) {
                    case ResetFormat:
                        charFormat = input.format;
                        endFormatScope();
                        break;
                    case BoldText:
                        charFormat.setFontWeight(QFont::Bold);
                        setFormatScope(charFormat);
                        break;
                    case DefaultTextColor:
                        charFormat.setForeground(input.format.foreground());
                        setFormatScope(charFormat);
                        break;
                    case DefaultBackgroundColor:
                        charFormat.setBackground(input.format.background());
                        setFormatScope(charFormat);
                        break;
                    case RgbTextColor:
                    case RgbBackgroundColor:
                        // See http://en.wikipedia.org/wiki/ANSI_escape_code#Colors
                        if (++i >= numbers.size())
                            break;
                        switch (numbers.at(i).toInt()) {
                        case 2:
                            // RGB 设置与格式: 38;2;<r>;<g>;<b>
                            if ((i + 3) < numbers.size()) {
                                (code == RgbTextColor) ?
                                      charFormat.setForeground(QColor(numbers.at(i + 1).toInt(),
                                                                      numbers.at(i + 2).toInt(),
                                                                      numbers.at(i + 3).toInt())) :
                                      charFormat.setBackground(QColor(numbers.at(i + 1).toInt(),
                                                                      numbers.at(i + 2).toInt(),
                                                                      numbers.at(i + 3).toInt()));
                                setFormatScope(charFormat);
                            }
                            i += 3;
                            break;
                        case 5:
                            // 256 带格式的彩色模式: 38;5;<i>
                            uint index = numbers.at(i + 1).toUInt();

                            QColor color;
                            if (index < 8) {
                                // 前8种颜色是标准的低强度ANSI颜色
                                color = ansiColor(index);
                            } else if (index < 16) {
                                // 接下来的8种颜色是标准的高强度ANSI颜色。
                                color = ansiColor(index - 8).lighter(150);
                            } else if (index < 232) {
                                //接下来的216种颜色是一个6x6x6 RGB的立方体。
                                uint o = index - 16;
                                color = QColor((o / 36) * 51, ((o / 6) % 6) * 51, (o % 6) * 51);
                            } else {
                                // 最后24种颜色是灰度渐变。
                                int grey = int((index - 232) * 11);
                                color = QColor(grey, grey, grey);
                            }

                            if (code == RgbTextColor)
                                charFormat.setForeground(color);
                            else
                                charFormat.setBackground(color);

                            setFormatScope(charFormat);
                            ++i;
                            break;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }
    return outputData;
}

void AnsiEscapeCodeHandler::endFormatScope()
{
    m_previousFormatClosed = true;
}

void AnsiEscapeCodeHandler::setFormatScope(const QTextCharFormat &charFormat)
{
    m_previousFormat = charFormat;
    m_previousFormatClosed = false;
}

} // namespace Utils
