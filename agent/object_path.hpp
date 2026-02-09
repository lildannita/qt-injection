#pragma once

#include <QObject>
#include <QString>

namespace agent {
QString getCorrectClassName(const QObject *obj) noexcept;
QString objectPath(const QObject *obj) noexcept;
} // namespace agent
