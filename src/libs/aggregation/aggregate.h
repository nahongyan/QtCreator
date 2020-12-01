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

#include "aggregation_global.h"

#include <QObject>
#include <QList>
#include <QHash>
#include <QReadWriteLock>
#include <QReadLocker>

/*
类库 Aggregation 只有一个类Aggregate以及很多辅助函数。这些都位于Aggregation命名空间中。
Aggregation命名空间包含用于“打包”相关组件的类和函数。
所谓“打包”，意思是，多个组件组成一个整体，将各自的属性和行为都暴露出来。这个“打包的整体”被称为Aggregate，
也就是Aggregation命名空间唯一的一个类。
*/
namespace Aggregation {

class AGGREGATION_EXPORT Aggregate : public QObject
{
    Q_OBJECT

public:
    Aggregate(QObject *parent = nullptr);
    ~Aggregate() override;

    void add(QObject *component);
    void remove(QObject *component);

/*!
这两个函数模板类似，只不过一个用于转换成一个对象，另外一个用于转换成一组对象。
函数使用读锁来保证线程安全，通过遍历m_components中每一个对象，
使用qobject_cast来判断是否所需要的类型，然后将符合的对象返回。
*/
    template <typename T> T *component() {
        QReadLocker locker(&lock());
        for (QObject *component : qAsConst(m_components)) {
            if (T *result = qobject_cast<T *>(component))
                return result;
        }
        return nullptr;
    }

    template <typename T> QList<T *> components() {
        QReadLocker locker(&lock());
        QList<T *> results;
        for (QObject *component : qAsConst(m_components)) {
            if (T *result = qobject_cast<T *>(component)) {
                results << result;
            }
        }
        return results;
    }

    static Aggregate *parentAggregate(QObject *obj);
    static QReadWriteLock &lock();

signals:
    void changed();

private:
    void deleteSelf(QObject *obj);

    static QHash<QObject *, Aggregate *> &aggregateMap();

    QList<QObject *> m_components;
};

//全局的查询函数的辅助函数
template <typename T> T *query(Aggregate *obj)
{
    if (!obj)
        return nullptr;
    return obj->template component<T>();
}

/*!
Aggregation命名空间提供了全局的查询函数，用于替代qobject_cast。
这些函数比qobject_cast更实用，因为它们支持Aggregate对象内部的查询
query()用于将obj对象转换成所需要的类型。首先，它会尝试使用qobject_cast进行转换，如果转换成功则直接返回，
否则会查询其是否存在于某个Aggregate对象，如果是，则从该对象继续尝试查询。
*/
template <typename T> T *query(QObject *obj)
{
    if (!obj)
        return nullptr;
    T *result = qobject_cast<T *>(obj);
    if (!result) {
        QReadLocker locker(&Aggregate::lock());
        Aggregate *parentAggregation = Aggregate::parentAggregate(obj);
        result = (parentAggregation ? query<T>(parentAggregation) : nullptr);
    }
    return result;
}

/*! 通过模板函数获取特定类型的所有组件
 Aggregation同样提供了返回QList<T>的query_all()函数，这与query()非常类似
*/
template <typename T> QList<T *> query_all(Aggregate *obj)
{
    if (!obj)
        return QList<T *>();
    return obj->template components<T>();
}

template <typename T> QList<T *> query_all(QObject *obj)
{
    if (!obj)
        return QList<T *>();
    QReadLocker locker(&Aggregate::lock());
    Aggregate *parentAggregation = Aggregate::parentAggregate(obj);
    QList<T *> results;
    if (parentAggregation)
        results = query_all<T>(parentAggregation);
    else if (T *result = qobject_cast<T *>(obj))
        results.append(result);
    return results;
}

} // namespace Aggregation
