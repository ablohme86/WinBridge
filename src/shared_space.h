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
#include <functional>

namespace WinBridge {

QString dataDir();
QString defaultPrefix();
QString settingsPath();
QJsonObject readSettings();
bool saveSettings(const QJsonObject &settings);

class SettingsLock {
    int fd = -1;
public:
    SettingsLock();
    ~SettingsLock();
    SettingsLock(const SettingsLock &) = delete;
    SettingsLock &operator=(const SettingsLock &) = delete;
    bool isLocked() const { return fd >= 0; }
};

QString sharedPrefix(const QJsonObject &settings = QJsonObject());

QString selectProton(
    const QStringList &versions,
    std::function<QString(const QStringList &)> chooser,
    std::function<void(const QString &)> validate,
    bool change = false
);

} // namespace WinBridge
