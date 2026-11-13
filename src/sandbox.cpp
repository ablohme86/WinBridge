/*
    WinBridge v1.0
    Copyright (c) 2026 A. Blohmè <alexander.blohme@gmail.com>
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "sandbox.h"
#include "shared_space.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QThread>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace WinBridge {

static const char *markerName = ".winbridge-sandbox";

static QString canonicalOrClean(const QString &path) {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

QString sandboxesDir() {
    return QDir::cleanPath(dataDir() + "/sandboxes");
}

QString createSandbox(const QString &exePath) {
    const QString base = sandboxesDir();
    if (!QDir().mkpath(base)) throw std::runtime_error("Could not create the WinBridge sandbox directory.");
    for (int attempt = 0; attempt < 16; ++attempt) {
        const QString name = QString("%1-%2")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"))
            .arg(QRandomGenerator::global()->generate() & 0xffffff, 6, 16, QChar('0'));
        const QString path = base + "/" + name;
        // mkdir without parents fails if the name is taken, so two launches
        // can never share a sandbox by accident.
        if (QDir(path).exists() || !QDir(base).mkdir(name)) continue;
        QFile marker(path + "/" + markerName);
        if (!marker.open(QIODevice::WriteOnly)) break;
        marker.write(QJsonDocument(QJsonObject{
            {"executable", exePath},
            {"created", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}
        }).toJson(QJsonDocument::Indented));
        marker.close();
        if (!QDir().mkpath(path + "/pfx")) break;
        return canonicalOrClean(path);
    }
    throw std::runtime_error("Could not create a WinBridge sandbox.");
}

bool isSandboxPrefix(const QString &path) {
    if (path.isEmpty()) return false;
    const QString candidate = canonicalOrClean(path);
    const QString base = canonicalOrClean(sandboxesDir());
    return QFileInfo(candidate).dir().absolutePath() == base &&
           QFileInfo(candidate + "/" + markerName).isFile();
}

void touchSandbox(const QString &path) {
    QFile marker(path + "/" + markerName);
    if (marker.open(QIODevice::ReadWrite)) {
        marker.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
    }
}

static bool insideSandbox(const QString &value, const QString &sandbox) {
    if (value.isEmpty()) return false;
    const QString clean = QDir::cleanPath(value);
    return clean == sandbox || clean.startsWith(sandbox + "/");
}

bool isSandboxInUse(const QString &path, const QString &procRoot) {
    const QString sandbox = canonicalOrClean(path);
    QDir procDir(procRoot.isEmpty() ? "/proc" : procRoot);
    for (const QString &pid : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        pid.toLongLong(&ok);
        if (!ok) continue;
        QFile environ(procDir.filePath(pid + "/environ"));
        if (!environ.open(QIODevice::ReadOnly)) continue;
        // Wine, wineserver, Proton and UMU all inherit these variables, so a
        // lingering wineserver also keeps the sandbox alive.
        for (const QByteArray &pair : environ.readAll().split('\0')) {
            const int eq = pair.indexOf('=');
            if (eq <= 0) continue;
            const QByteArray key = pair.left(eq);
            if ((key == "STEAM_COMPAT_DATA_PATH" || key == "WINEPREFIX") &&
                insideSandbox(QString::fromUtf8(pair.mid(eq + 1)), sandbox)) {
                return true;
            }
        }
    }
    return false;
}

QStringList existingSandboxes() {
    QStringList result;
    QDir base(sandboxesDir());
    for (const QString &name : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name)) {
        const QString path = base.filePath(name);
        if (isSandboxPrefix(path)) result << canonicalOrClean(path);
    }
    return result;
}

QStringList cleanupSandboxes(qint64 idleSeconds, const QString &procRoot, const QDateTime &nowArg) {
    const QDateTime now = nowArg.isValid() ? nowArg : QDateTime::currentDateTime();
    QStringList removed;
    for (const QString &sandbox : existingSandboxes()) {
        if (isSandboxInUse(sandbox, procRoot)) {
            touchSandbox(sandbox);
            continue;
        }
        const QDateTime lastUsed = QFileInfo(sandbox + "/" + markerName).lastModified();
        if (lastUsed.isValid() && lastUsed.secsTo(now) < idleSeconds) continue;
        // removeRecursively() deletes symlinks such as dosdevices/z: without
        // following them, so nothing outside the sandbox is touched.
        if (QDir(sandbox).removeRecursively()) removed << sandbox;
    }
    return removed;
}

void ensureSandboxCleanup(const QString &launcher) {
    if (existingSandboxes().isEmpty() || launcher.isEmpty()) return;
    QProcess::startDetached(launcher, {"--cleanup-sandboxes"});
}

int runSandboxCleanup() {
    if (!QDir().mkpath(sandboxesDir())) return 1;
    const QByteArray lockPath = (sandboxesDir() + "/.cleanup.lock").toUtf8();
    do {
        const int fd = ::open(lockPath.constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) return 1;
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
            ::close(fd);
            return 0; // Another cleanup process is already watching.
        }
        while (true) {
            cleanupSandboxes();
            if (existingSandboxes().isEmpty()) break;
            QThread::sleep(30);
        }
        ::flock(fd, LOCK_UN);
        ::close(fd);
        // A launch that created a sandbox while this process held the lock
        // could not start its own cleanup process, so look once more.
    } while (!existingSandboxes().isEmpty());
    return 0;
}

} // namespace WinBridge
