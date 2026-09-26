/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QList>
#include <QString>

namespace WinBridge {

QString trayPrefix(const QString &prefixArg = "");
bool raiseTaskWindow(const QList<qint64> &pids, const QString &exeName);
int runTray(const QString &prefixArg = "");

} // namespace WinBridge
