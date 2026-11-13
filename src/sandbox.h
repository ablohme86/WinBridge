/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace WinBridge {

// A sandbox is a disposable compatdata directory under sandboxesDir(). It is
// marked with a file whose modification time records when a Windows program
// last ran in it, so idle sandboxes can be removed without touching the shared
// WinBridge environment.
constexpr qint64 sandboxIdleSeconds = 10 * 60;

QString sandboxesDir();
QString createSandbox(const QString &exePath);
bool isSandboxPrefix(const QString &path);
void touchSandbox(const QString &path);
bool isSandboxInUse(const QString &path, const QString &procRoot = "/proc");
QStringList existingSandboxes();
QStringList cleanupSandboxes(
    qint64 idleSeconds = sandboxIdleSeconds,
    const QString &procRoot = "/proc",
    const QDateTime &now = QDateTime()
);

// Starts a detached cleanup process when sandboxes exist. Only one cleanup
// process runs at a time; it exits once every sandbox has been removed.
void ensureSandboxCleanup(const QString &launcher);
int runSandboxCleanup();

} // namespace WinBridge
