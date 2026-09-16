/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>

class QWidget;

namespace WinBridge {

QString recentExecutablesPath();
QStringList recentExecutables(const QString &customPath = "");
bool rememberExecutable(const QString &exePath, const QString &customPath = "");
QString managerExecutablePath(const QString &launcherPath = "");

struct OpenRequest {
    bool accepted = false;
    bool screenshotSaved = false;
    QString executable;
};

OpenRequest showExecutableOpener(
    const QString &launcher,
    QWidget *parent = nullptr,
    const QString &screenshotPath = "",
    const QString &theme = ""
);

} // namespace WinBridge
