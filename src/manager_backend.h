/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://gnu.org>.
*/

#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QPair>

namespace WinBridge {

QList<QPair<QString, QString>> parsePrograms(const QString &output);

QMap<QString, QList<qint64>> getRunningApps(
    const QString &prefix,
    const QJsonArray &programs = QJsonArray(),
    const QString &procRoot = "/proc"
);

QJsonObject killProgram(
    const QString &prefix,
    const QString &key,
    const QJsonArray &programs = QJsonArray(),
    const QString &procRoot = "/proc"
);

QMap<QString, QString> programIcons(const QString &prefix);
QString programInstallDirectory(const QString &prefix, const QString &location);

QJsonObject operate(
    const QString &action,
    const QString &key = QString(),
    const QString &selectedProton = QString(),
    const QString &selectedPrefix = QString(),
    const QString &shortcutId = QString(),
    const QVariant &desktop = QVariant(),
    const QVariant &menu = QVariant()
);

} // namespace WinBridge
