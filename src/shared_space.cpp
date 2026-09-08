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

#include "shared_space.h"
#include "launcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace WinBridge {

QString dataDir() {
    QString base = qEnvironmentVariable("XDG_DATA_HOME");
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.local/share";
    }
    return QDir::cleanPath(base + "/winbridge");
}

QString defaultPrefix() {
    return QDir::cleanPath(dataDir() + "/shared");
}

QString settingsPath() {
    QString conf = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (conf.isEmpty()) {
        conf = QDir::homePath() + "/.config";
    }
    return QDir::cleanPath(conf + "/winbridge/settings.json");
}

QJsonObject readSettings() {
    QString path = settingsPath();
    QFile file(path);
    if (!file.exists()) {
        QDir confDir = QFileInfo(path).dir();
        confDir.cdUp();
        QString legacyPath = QDir::cleanPath(confDir.absolutePath() + "/protonrun/settings.json");
        file.setFileName(legacyPath);
        if (!file.exists()) {
            return QJsonObject();
        }
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        throw std::runtime_error("Ugyldige WinBridge-innstillinger.");
    }

    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!it.value().isString()) {
            throw std::runtime_error("Ugyldige WinBridge-innstillinger.");
        }
    }
    return obj;
}

bool saveSettings(const QJsonObject &settings) {
    QString path = settingsPath();
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QTemporaryFile temp(path + ".tmp.XXXXXX");
    if (!temp.open()) {
        return false;
    }

    QJsonDocument doc(settings);
    temp.write(doc.toJson(QJsonDocument::Indented));
    temp.flush();
    QString tempPath = temp.fileName();
    temp.close();

    QFile::remove(path);
    return QFile::rename(tempPath, path);
}

SettingsLock::SettingsLock() {
    QString path = settingsPath() + ".lock";
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    fd = ::open(path.toUtf8().constData(), O_CREAT | O_RDWR, 0666);
    if (fd >= 0) {
        ::flock(fd, LOCK_EX);
    }
}

SettingsLock::~SettingsLock() {
    if (fd >= 0) {
        ::flock(fd, LOCK_UN);
        ::close(fd);
    }
}

QString sharedPrefix(const QJsonObject &passedSettings) {
    QJsonObject settings = passedSettings;
    if (settings.isEmpty()) {
        try {
            settings = readSettings();
        } catch (...) {
            settings = QJsonObject();
        }
    }

    if (settings.contains("prefix") && !settings["prefix"].toString().trimmed().isEmpty()) {
        QString p = settings["prefix"].toString().trimmed();
        if (p.startsWith("~/")) {
            p = QDir::homePath() + p.mid(1);
        }
        QFileInfo fi(p);
        return fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath();
    }

    QString oldData = QDir::cleanPath(dataDir() + "/../protonrun");
    if (QDir(oldData + "/shared/pfx").exists()) {
        QFileInfo fi(oldData + "/shared");
        return fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath();
    }

    QDir oldPrefixesDir(oldData + "/prefixes");
    if (oldPrefixesDir.exists()) {
        QStringList entries = oldPrefixesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QStringList legacy;
        for (const QString &e : entries) {
            if (QDir(oldPrefixesDir.filePath(e + "/pfx")).exists()) {
                QFileInfo fi(oldPrefixesDir.filePath(e));
                legacy << (fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath());
            }
        }
        if (legacy.size() == 1) {
            return legacy[0];
        }
    }

    QDir curPrefixesDir(dataDir() + "/prefixes");
    if (curPrefixesDir.exists()) {
        QStringList entries = curPrefixesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        QStringList curLegacy;
        for (const QString &e : entries) {
            if (QDir(curPrefixesDir.filePath(e + "/pfx")).exists()) {
                QFileInfo fi(curPrefixesDir.filePath(e));
                curLegacy << (fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath());
            }
        }
        if (curLegacy.size() == 1) {
            return curLegacy[0];
        }
    }

    return defaultPrefix();
}

QString selectProton(
    const QStringList &versions,
    std::function<QString(const QStringList &)> chooser,
    std::function<void(const QString &)> validate,
    bool change
) {
    SettingsLock lock;
    QJsonObject settings;
    try {
        settings = readSettings();
    } catch (...) {
        settings = QJsonObject();
    }

    QString saved = settings.value("proton").toString();
    if (!saved.isEmpty() && isProtonAvailable(saved) && !change) {
        return saved;
    }

    if (versions.isEmpty()) {
        throw std::runtime_error("Fant ingen Proton-versjoner. Last ned Proton i innstillingene til WinBridge Manager.");
    }

    QString selected = chooser(versions);
    if (selected.isEmpty()) {
        return QString();
    }

    validate(selected);
    settings["prefix"] = sharedPrefix(settings);
    settings["proton"] = selected;
    saveSettings(settings);
    return selected;
}

} // namespace WinBridge
