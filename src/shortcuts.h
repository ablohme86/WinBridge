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
#include <QVariant>

namespace WinBridge {

QString desktopQuote(const QString &value);
QString field(const QString &value);
QString desktopDir();
QString enclosingPrefix(const QString &exePath);
QString shortcutTarget(const QString &value, const QString &prefix);

QString shortcutsConfigPath();
QJsonObject readShortcutsConfig();
bool saveShortcutsConfig(const QJsonObject &config);

QString shortcutIconPath(const QString &sourceDir, const QString &iconName);

struct ShortcutInfo {
    QString id;
    QString name;
    QString file;
    QString icon;
    QString path;
    QString exec;
    bool desktop = true;
    bool menu = true;
};

QList<ShortcutInfo> loadShortcuts(const QString &prefix, const QString &dataDir = "", const QString &desktop = "");

struct ProgramRegistryMeta {
    QString key;
    QString name;
    QString loc;
    QString group;
    QString icon;
};

QMap<QString, ProgramRegistryMeta> getProgramRegistryMeta(const QString &prefix);

QJsonArray groupShortcutsByProgram(const QString &prefix, const QJsonArray &programs);

QJsonObject toggleShortcut(
    const QString &prefix,
    const QString &shortcutId,
    const QVariant &desktop = QVariant(),
    const QVariant &menu = QVariant(),
    const QString &launcher = "",
    const QString &proton = "",
    const QString &customDataDir = "",
    const QString &customDesktopDir = ""
);

QStringList importShortcuts(
    const QString &prefix,
    const QString &proton,
    const QString &launcher,
    const QString &dataDir = "",
    const QString &desktopDir = ""
);

} // namespace WinBridge
