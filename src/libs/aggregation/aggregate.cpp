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

#include "aggregate.h"

#include <QWriteLocker>
#include <QDebug>

/*!
    \namespace Aggregation
    \inmodule QtCreator
    \brief 聚合命名空间包含对绑定相关组件的支持，控件的属性和行为其他的成分到外面。
绑定到聚合中的组件可以相互转换并且有一个耦合的生命周期。请参阅聚合::聚合的文档细节和例子。
*/

/*!
    \class Aggregation::Aggregate
    \inheaderfile aggregation/aggregate.h
    \inmodule QtCreator
    \ingroup mainclasses
    \threadsafe

    \brief 聚合类定义了相关组件的集合可以看作是一个单位。
聚合是作为单元处理的组件的集合，控件的属性和行为其他组成部分聚集到外面。
特别是这意味着:
    \list
    \li 它们可以相互转换(使用query()和query_all()功能)。
    \li 它们的生命周期是耦合的。也就是说，只要删除一个，所有的他们是。
    \endlist
组件可以是任何QObject派生类型。可以使用聚合通过聚合模拟多重继承。假设我们有以下代码:
    \code
        using namespace Aggregation;
        class MyInterface : public QObject { ........ };
        class MyInterfaceEx : public QObject { ........ };
        [...]
        MyInterface *object = new MyInterface; // 这是单遗传
    \endcode
    查询函数的工作方式类似于普通对象的qobject_cast():
    \code
        Q_ASSERT(query<MyInterface>(object) == object);
        Q_ASSERT(query<MyInterfaceEx>(object) == 0);
    \endcode
如果我们想让object也实现MyInterfaceEx类，但如果不想或不能使用多重继承，我们可以这样做
在任何时候使用聚合:
    \code
        MyInterfaceEx *objectEx = new MyInterfaceEx;
        Aggregate *aggregate = new Aggregate;
        aggregate->add(object);
        aggregate->add(objectEx);
    \endcode
聚合将这两个对象捆绑在一起。
如果我们有任何部分的集合，我们得到所有部分:
    \code
        Q_ASSERT(query<MyInterface>(object) == object);
        Q_ASSERT(query<MyInterfaceEx>(object) == objectEx);
        Q_ASSERT(query<MyInterface>(objectEx) == object);
        Q_ASSERT(query<MyInterfaceEx>(objectEx) == objectEx);
    \endcode
    下面将删除所有这三个:object、 objectEx和aggregate ,删除任意一个:
    \code
        delete objectEx;
        // or delete object;
        // or delete aggregate;
    \endcode

支持聚合的代码从不使用qobject_cast()。它总是使用query()，它的行为类似于qobject_cast()作为回退。
*/

/*!
    \fn template <typename T> T *Aggregation::Aggregate::component()
模板函数，返回具有给定类型的组件(如果有的话)。如果有多个具有该类型的组件，则返回一个随机的组件。

    \sa Aggregate::components(), add()
*/

/*!
    \fn template <typename T> QList<T *> Aggregation::Aggregate::components()

   模板函数，返回具有给定类型的所有组件(如果有的话)。

    \sa Aggregate::component(), add()
*/

/*!
    \relates Aggregation::Aggregate
    \fn template <typename T> T *Aggregation::query<T *>(QObject *obj)

执行动态转换，该转换意识到一个可能的聚集\一个obj
可能属于。如果obj本身是被请求的类型，它只是被强制转换
并返回。否则，如果obj属于一个集合，那么它的所有组件都是
检查。如果它不属于聚合，则返回null。

    \sa Aggregate::component()
*/

/*!
    \relates Aggregation::Aggregate
    \fn template <typename T> QList<T *> Aggregation::query_all<T *>(QObject *obj)

如果obj属于一个集合，所有的组件都可以转换为给定的
返回类型。否则，如果obj是被请求的类型，则返回一个obj。

    \sa Aggregate::components()
*/

/*!
    \fn void Aggregation::Aggregate::changed()
当组件被添加到组件或从组件中移除组件时，该信号就会发出聚合
    \sa add(), remove()
*/

using namespace Aggregation;

/*!
    返回obj的聚合对象(如果有的话)。否则返回0。
*/
Aggregate *Aggregate::parentAggregate(QObject *obj)
{
    QReadLocker locker(&lock());
    return aggregateMap().value(obj);
}

/*!
 * 它是一个散列，保存每个QObject及其子类与Aggregate的对应关系。
*/
QHash<QObject *, Aggregate *> &Aggregate::aggregateMap()
{
    static QHash<QObject *, Aggregate *> map;
    return map;
}

/*!
    \internal
*/
QReadWriteLock &Aggregate::lock()
{
    static QReadWriteLock lock;
    return lock;
}

/*!
由于Aggregate是QObject的子类，所以Aggregate构造函数需要一个QObject参数。
QWriteLocker加了一个写锁，用于保证在多线程情景下依然能够正常插入。
QWriteLocker简化了QReadWriteLock锁的写操作，它需要一个QReadWriteLock锁作为参数，而这个QReadWriteLock正是lock()函数返回的
aggregateMap()函数是一个私有静态函数因为Aggregate本身就是QObject，因此使用aggregateMap().insert(this, this);
表示Aggregate本身隶属于其自己这个聚合体。
由于这个函数是静态的，所以所有Aggregate共享一个散列，起到统一注册管理的作用，无需引入第三个管理类来管理这些Aggregate。
*/
Aggregate::Aggregate(QObject *parent)
    : QObject(parent)
{
    QWriteLocker locker(&lock());
    aggregateMap().insert(this, this);
}

/*!
在Aggregate的析构函数中，我们需要遍历m_components中的每一个对象，断开其关联的信号槽，
然后从aggregateMap()中移除。QList的清空操作的确会令人疑惑。
如果QList中保存的是指针，就像这里，那么，清空QList需要两个步骤：调用clear()函数和qDeleteAll()。
前者用于将指针从QList移除，后者会针对每一个指针都调用delete运算符。
因为QList并不会持有这些指针指向的内存，所以，每一个元素都需要单独调用delete。
这里比较令人不解的是，为什么要将m_components赋值给一个新的QList对象components，然后再调用qDeleteAll()。
豆子这里也并不大明白，只是猜测，这是为了将qDeleteAll()的调用移动到写锁之外，毕竟这个操作不需要写同步，而delete可能会比较耗时。
如果有不同意见，可以探讨一下
*/
Aggregate::~Aggregate()
{
    QList<QObject *> components;
    {
        QWriteLocker locker(&lock());
        for (QObject *component : qAsConst(m_components)) {
            disconnect(component, &QObject::destroyed, this, &Aggregate::deleteSelf);
            aggregateMap().remove(component);
        }
        components = m_components;
        m_components.clear();
        aggregateMap().remove(this);
    }
    qDeleteAll(components);
}
/*!
移除操作的槽函数在QObject对象发出destroyed()信号，也就是对象析构时会被调用。
由于移除操作也是一种写操作，所以这里还是需要写锁。
对象被析构，为避免内存泄露，我们需要自己从aggregateMap()和m_components中将其移除。
最后，为了实现聚合体内部任意组件被析构，聚合体本身也要被析构这一特性，函数最后做了delete this操作
*/
void Aggregate::deleteSelf(QObject *obj)
{
    {
        QWriteLocker locker(&lock());
        aggregateMap().remove(obj);
        m_components.removeAll(obj);
    }
    delete this;
}
/*!
add()函数用于向Aggregate中添加组件。添加是写操作，所以还是需要写锁。
首先，查找需要添加的这个component是否已经在aggregateMap()添加，并且添加到的Aggregate就是this自己。
如果是的话，不需要作任何操作，直接返回；如果不是，则会给出一个警告，“这个组件已经被添加到另外的聚合体”。
如果该component没有被添加到任何一个聚合体，则添加到自己的m_components属性，并且关联销毁的信号槽，
当对象析构时会发出destroyed()信号时，会调用聚合体的deleteSelf()槽函数
最后还需要到aggregateMap()注册。m_components的定义如下： QList<QObject *> m_components;
*/
void Aggregate::add(QObject *component)
{
    if (!component)
        return;
    {
        QWriteLocker locker(&lock());
        Aggregate *parentAggregation = aggregateMap().value(component);
        if (parentAggregation == this)
            return;
        if (parentAggregation) {
            qWarning() << "无法添加属于不同聚合的组件" << component;
            return;
        }
        m_components.append(component);
        connect(component, &QObject::destroyed, this, &Aggregate::deleteSelf);
        aggregateMap().insert(component, this);
    }
    emit changed();
}

/*!
remove()函数用于移除聚合体中的组件。其实现与add()类似。
在添加了写锁之后，首先从aggregateMap()中移除对应的组件，然后将其从自己的m_components中全部删除，最后再将信号槽断开连接。
最后一个断开的操作是必要的，因为我们只是从聚合体中移除对象，
当对象析构时，我们不希望其所在聚合体也会被析构（这是我们在add()操作中关联的）

*/
void Aggregate::remove(QObject *component)
{
    if (!component)
        return;
    {
        QWriteLocker locker(&lock());
        aggregateMap().remove(component);
        m_components.removeAll(component);
        disconnect(component, &QObject::destroyed, this, &Aggregate::deleteSelf);
    }
    emit changed();
}
