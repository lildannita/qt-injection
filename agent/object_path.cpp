#include "object_path.hpp"

#include <QQuickItem>
#include <QRegularExpression>
#include <QStringList>
#include <cassert>
#include <utility>

namespace agent {
QString getCorrectClassName(const QObject *obj) noexcept
{
    assert(obj != nullptr);
    static const auto s_qmlRegExp = QRegularExpression(
        QStringLiteral(
            "(?<=.)(_(QMLTYPE|QML)_\\d+)"
            "|(_QMLTYPE_\\d+_QML_\\d+)"
            "|(_QML_\\d+_QMLTYPE_\\d+)$"));
    return QString::fromLatin1(obj->metaObject()->className()).remove(s_qmlRegExp);
}

static std::pair<const QObject *, QObjectList> getParentInfo(const QObject *obj) noexcept
{
    assert(obj != nullptr);
    if (const auto *classicParent = obj->parent()) {
        return { classicParent, classicParent->children() };
    }
    if (const auto *quickItem = qobject_cast<const QQuickItem *>(obj)) {
        if (const auto *itemParent = quickItem->parentItem()) {
            QObjectList siblings;
            for (auto *child : itemParent->childItems()) {
                siblings.push_back(child);
            }
            assert(siblings.contains(const_cast<QObject *>(obj)));
            return { qobject_cast<const QObject *>(itemParent), std::move(siblings) };
        }
    }
    return { nullptr, {} };
}

static uint indexAmongSameClassSiblings(const QObject *obj, const QString &className, const QObjectList &siblings) noexcept
{
    assert(obj != nullptr);
    assert(!siblings.isEmpty());

    uint index = 0;
    for (const auto *child : siblings) {
        if (child == obj)
            return index;
        if (getCorrectClassName(child) == className)
            ++index;
    }
    Q_UNREACHABLE();
}

static QString nodeLabel(const QObject *obj, const QObject *parent, const QObjectList &siblings) noexcept
{
    assert(obj != nullptr);

    const auto &name = obj->objectName();
    if (!name.isEmpty()) {
        return name;
    }

    auto className = getCorrectClassName(obj);
    if (parent) {
        const auto idx = indexAmongSameClassSiblings(obj, className, siblings);
        if (idx > 0) {
            return QStringLiteral("%1_%2").arg(className).arg(idx);
        }
    }
    return className;
}

QString objectPath(const QObject *obj) noexcept
{
    assert(obj != nullptr);

    QStringList components;
    const auto *current = obj;
    while (current) {
        const auto [parent, siblings] = getParentInfo(current);
        components.prepend(nodeLabel(current, parent, siblings));
        current = parent;
    }
    assert(!components.isEmpty());
    return QLatin1Char('/') + components.join(QLatin1Char('/'));
}
} // namespace agent
