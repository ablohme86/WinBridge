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
#include <QMap>

namespace WinBridge {

QMap<QString, QString> parseKeyValuePairs(const QString &filePath);
QStringList steamRoots();
QStringList steamLibraries(const QStringList &roots);
QStringList discoverProtons(const QStringList &roots, const QStringList &libs);
QStringList protonChoices(const QStringList &roots, const QStringList &libs);
bool isProtonDownload(const QString &proton);
bool isProtonAvailable(const QString &proton);
QString protonName(const QString &proton);
QString umuExecutable();
void validateProton(const QString &proton, const QStringList &libs);
void installProton(const QString &proton);
QString runtimeFor(const QString &protonDir, const QStringList &libs);

QString dialogTool();
void showError(const QString &message);
QString chooseProton(const QStringList &versions, const QString &exeName);

int launch(
    const QString &exe,
    const QString &proton,
    const QStringList &roots,
    const QStringList &libs,
    const QStringList &extra = {},
    const QString &prefix = "",
    bool capture = false,
    const QString &verb = "run",
    QString *capturedOutput = nullptr,
    const QString &launcher = ""
);

} // namespace WinBridge
