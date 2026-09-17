/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDialog>
#include <QSet>
#include <QString>
#include <QStringList>

class QWidget;

namespace WinBridge {

QString recentExecutablesPath();
QStringList recentExecutables(const QString &customPath = "");
bool rememberExecutable(const QString &exePath, const QString &customPath = "");
bool removeRecentExecutable(const QString &exePath, const QString &customPath = "");
QString managerExecutablePath(const QString &launcherPath = "");
QStringList parseArguments(const QString &argsText);

struct OpenRequest {
    bool accepted = false;
    bool screenshotSaved = false;
    QString executable;
    QString argumentsText;
    QStringList arguments;
};

OpenRequest showExecutableOpener(
    const QString &launcher,
    QWidget *parent = nullptr,
    const QString &screenshotPath = "",
    const QString &theme = "",
    const QString &recentStoragePath = ""
);

class AppLoadingDialog final : public QDialog {
public:
    explicit AppLoadingDialog(const QString &exePath, const QString &theme = "", QWidget *parent = nullptr);
};

QSet<qint64> matchingProcessPids(
    const QString &exePath,
    const QString &prefix = "",
    const QString &procRoot = "/proc"
);

QSet<unsigned long> activeWindowIds();

bool isExecutableStarted(
    const QString &exePath,
    const QString &prefix = "",
    const QSet<qint64> &initialPids = {},
    const QString &procRoot = "/proc"
);

bool isExecutableWindowVisible(
    const QString &exePath,
    const QString &prefix = "",
    const QSet<qint64> &initialPids = {},
    const QSet<unsigned long> &initialWindows = {},
    const QString &procRoot = "/proc"
);

int launchWithLoadingDialog(
    const QString &exe,
    const QString &proton,
    const QStringList &roots,
    const QStringList &libs,
    const QStringList &extra = {},
    const QString &prefix = "",
    const QString &theme = "",
    const QString &launcher = ""
);

} // namespace WinBridge
