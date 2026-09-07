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

#include "manager_backend.h"
#include "shared_space.h"
#include "shortcuts.h"
#include "launcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <signal.h>
#include <stdexcept>
#include <unistd.h>

namespace WinBridge {

static const QSet<QString> SYSTEM_PROCESSES = {
    "wineserver", "services.exe", "winedevice.exe", "plugplay.exe",
    "svchost.exe", "rpcss.exe", "conhost.exe", "tabtip.exe", "xalia.exe", "explorer.exe"
};

QList<QPair<QString, QString>> parsePrograms(const QString &output) {
    QList<QPair<QString, QString>> result;
    QSet<QString> seenKeys;
    const QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        if (line.contains("|||")) {
            int idx = line.indexOf("|||");
            QString key = line.left(idx).trimmed();
            QString name = line.mid(idx + 3).trimmed();
            if (!key.isEmpty() && !name.isEmpty() && !seenKeys.contains(key)) {
                seenKeys.insert(key);
                result.append(qMakePair(key, name));
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) {
        return a.second.toLower() < b.second.toLower();
    });
    return result;
}

struct ProcessDetail {
    qint64 pid = 0;
    qint64 ppid = 0;
    QStringList cmdline;
    QMap<QString, QString> env;
};

QMap<QString, QList<qint64>> getRunningApps(
    const QString &prefix,
    const QJsonArray &programsInput,
    const QString &procRoot
) {
    QString pfx = QDir::cleanPath(prefix);
    QString winePfx = pfx + "/pfx";
    QDir procDir(procRoot);
    QMap<QString, QList<qint64>> runningByKey;
    if (!procDir.exists()) return runningByKey;

    auto meta = getProgramRegistryMeta(pfx);
    QJsonArray programs = programsInput;
    if (programs.isEmpty()) {
        for (const auto &m : meta) {
            QJsonObject obj;
            obj["key"] = m.key;
            obj["name"] = m.name;
            programs.append(obj);
        }
    }

    QMap<qint64, QList<qint64>> childrenMap;
    QMap<qint64, ProcessDetail> procInfo;

    const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pidStr : pids) {
        bool isNumber = false;
        qint64 pid = pidStr.toLongLong(&isNumber);
        if (!isNumber) continue;

        QString pDir = procDir.filePath(pidStr);
        QFile statusFile(pDir + "/status");
        qint64 ppid = 0;
        if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!statusFile.atEnd()) {
                QString line = QString::fromUtf8(statusFile.readLine());
                if (line.startsWith("PPid:")) {
                    ppid = line.mid(5).trimmed().toLongLong();
                    break;
                }
            }
            statusFile.close();
        }

        childrenMap[ppid].append(pid);

        QFile envFile(pDir + "/environ");
        QMap<QString, QString> env;
        if (envFile.open(QIODevice::ReadOnly)) {
            QByteArray envBytes = envFile.readAll();
            envFile.close();
            const QList<QByteArray> pairs = envBytes.split('\0');
            for (const QByteArray &pair : pairs) {
                int eq = pair.indexOf('=');
                if (eq > 0) {
                    env[QString::fromUtf8(pair.left(eq))] = QString::fromUtf8(pair.mid(eq + 1));
                }
            }
        }

        QString dataPath = env.value("STEAM_COMPAT_DATA_PATH");
        QString envWinePfx = env.value("WINEPREFIX");
        bool inPrefix = false;
        if (!dataPath.isEmpty() && QDir::cleanPath(dataPath) == pfx) {
            inPrefix = true;
        } else if (!envWinePfx.isEmpty() && QDir::cleanPath(envWinePfx) == winePfx) {
            inPrefix = true;
        }

        if (!inPrefix) continue;

        QFile cmdFile(pDir + "/cmdline");
        QStringList cmdline;
        if (cmdFile.open(QIODevice::ReadOnly)) {
            QByteArray cmdBytes = cmdFile.readAll();
            cmdFile.close();
            const QList<QByteArray> parts = cmdBytes.split('\0');
            for (const QByteArray &p : parts) {
                if (!p.isEmpty()) {
                    cmdline << QString::fromUtf8(p);
                }
            }
        }

        ProcessDetail detail;
        detail.pid = pid;
        detail.ppid = ppid;
        detail.cmdline = cmdline;
        detail.env = env;
        procInfo[pid] = detail;
    }

    auto getDescendants = [&](qint64 rootPid) -> QSet<qint64> {
        QSet<qint64> desc;
        QList<qint64> queue = {rootPid};
        while (!queue.isEmpty()) {
            qint64 curr = queue.takeFirst();
            for (qint64 child : childrenMap.value(curr)) {
                if (!desc.contains(child)) {
                    desc.insert(child);
                    queue.append(child);
                }
            }
        }
        return desc;
    };

    for (const auto &progVal : programs) {
        QJsonObject prog = progVal.toObject();
        QString key = prog.value("key").toString();
        QString name = prog.value("name").toString();
        auto m = meta.value(key.toLower());

        QString loc = m.loc.toLower();
        QString group = m.group.toLower();
        QString icon = m.icon.toLower();

        QStringList scNames;
        const QJsonArray scArr = prog.value("shortcuts").toArray();
        for (const auto &sc : scArr) {
            scNames << sc.toObject().value("name").toString().toLower();
        }

        QSet<qint64> matchedPids;

        for (auto it = procInfo.constBegin(); it != procInfo.constEnd(); ++it) {
            const ProcessDetail &info = it.value();
            if (info.cmdline.isEmpty()) continue;

            QString base = QFileInfo(QString(info.cmdline[0]).replace(R"(\\)", "/")).fileName().toLower();
            if (SYSTEM_PROCESSES.contains(base)) continue;

            QString cmdStr = info.cmdline.join(" ").toLower().replace(R"(\\)", "/");
            bool matched = false;

            if (!loc.isEmpty() && cmdStr.contains(loc)) matched = true;
            else if (!group.isEmpty() && cmdStr.contains(group)) matched = true;
            else if (icon.length() > 3 && cmdStr.contains(icon)) matched = true;
            else if (name.length() > 3 && cmdStr.contains(name.toLower())) matched = true;
            else if (key.length() > 3 && cmdStr.contains(key.toLower())) matched = true;
            else {
                for (const QString &s : scNames) {
                    if (s.length() > 3 && cmdStr.contains(s)) {
                        matched = true;
                        break;
                    }
                }
            }

            if (matched) {
                matchedPids.insert(info.pid);
                for (qint64 descPid : getDescendants(info.pid)) {
                    if (procInfo.contains(descPid)) {
                        QString descBase = procInfo[descPid].cmdline.isEmpty() ? "" : QFileInfo(QString(procInfo[descPid].cmdline[0]).replace(R"(\\)", "/")).fileName().toLower();
                        if (!SYSTEM_PROCESSES.contains(descBase)) {
                            matchedPids.insert(descPid);
                        }
                    }
                }
            }
        }

        QList<qint64> sortedPids = matchedPids.values();
        std::sort(sortedPids.begin(), sortedPids.end());
        runningByKey[key] = sortedPids;
    }

    return runningByKey;
}

QJsonObject killProgram(
    const QString &prefix,
    const QString &key,
    const QJsonArray &programs,
    const QString &procRoot
) {
    QString pfx = QDir::cleanPath(prefix);
    auto runningMap = getRunningApps(pfx, programs, procRoot);
    QList<qint64> pids = runningMap.value(key);

    if (pids.isEmpty()) {
        for (auto it = runningMap.constBegin(); it != runningMap.constEnd(); ++it) {
            if (it.key().toLower() == key.toLower()) {
                pids = it.value();
                break;
            }
        }
    }

    if (pids.isEmpty()) {
        QJsonObject res;
        res["killed"] = false;
        res["key"] = key;
        res["pids"] = QJsonArray();
        res["error"] = "Program is not running.";
        return res;
    }

    for (qint64 pid : pids) {
        ::kill(pid, SIGTERM);
    }
    QThread::msleep(100);
    for (qint64 pid : pids) {
        ::kill(pid, SIGKILL);
    }

    QJsonArray pidsArr;
    for (qint64 pid : pids) pidsArr.append(pid);

    QJsonObject res;
    res["killed"] = true;
    res["key"] = key;
    res["pids"] = pidsArr;
    return res;
}

QMap<QString, QString> programIcons(const QString &prefix) {
    QString pfx = QDir::cleanPath(prefix);
    QString source = pfx + "/pfx/drive_c/proton_shortcuts";
    QDir srcDir(source);
    QMap<QString, QString> result;

    for (const QString &file : srcDir.entryList({"*.desktop"}, QDir::Files, QDir::Name)) {
        QFile scFile(srcDir.filePath(file));
        if (!scFile.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString name, icon;
        bool inDesktopEntry = false;
        while (!scFile.atEnd()) {
            QString line = QString::fromUtf8(scFile.readLine()).trimmed();
            if (line.startsWith('[')) {
                inDesktopEntry = (line == "[Desktop Entry]");
                continue;
            }
            if (!inDesktopEntry || line.startsWith('#') || !line.contains('=')) continue;
            int idx = line.indexOf('=');
            QString k = line.left(idx).trimmed();
            QString v = line.mid(idx + 1).trimmed();
            if (k == "Name") name = v.toLower();
            else if (k == "Icon") icon = v;
        }

        if (name.isEmpty() || icon.isEmpty() || QFileInfo(icon).fileName() != icon) continue;

        QDir iconsDir(source + "/icons");
        if (!iconsDir.exists()) continue;

        QString bestPath;
        int maxRes = -1;
        for (const QString &sub : iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString p = iconsDir.filePath(sub + "/apps/" + icon + ".png");
            QFileInfo fi(p);
            if (fi.isFile()) {
                int res = 0;
                if (sub.contains('x')) {
                    res = sub.split('x')[0].toInt();
                }
                if (res >= maxRes) {
                    maxRes = res;
                    bestPath = fi.canonicalFilePath().isEmpty() ? QDir::cleanPath(fi.absoluteFilePath()) : fi.canonicalFilePath();
                }
            }
        }

        if (!bestPath.isEmpty()) {
            result[name] = bestPath;
        }
    }
    return result;
}

QJsonObject operate(
    const QString &action,
    const QString &key,
    const QString &selectedProton,
    const QString &selectedPrefix,
    const QString &shortcutId,
    const QVariant &desktop,
    const QVariant &menu
) {
    QJsonObject settings;
    try {
        settings = readSettings();
    } catch (...) {
        settings = QJsonObject();
    }

    if (action == "settings" || action == "configure") {
        QStringList roots = steamRoots();
        QStringList libs = steamLibraries(roots);
        QStringList versions = discoverProtons(roots, libs);

        if (action == "configure") {
            SettingsLock lock;
            try {
                settings = readSettings();
            } catch (...) {
                settings = QJsonObject();
            }

            if (!selectedProton.isEmpty()) {
                QFileInfo protonFi(selectedProton);
                QString selected = protonFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(protonFi.absoluteFilePath()) : protonFi.canonicalFilePath();
                if (!versions.contains(selected)) {
                    throw std::runtime_error("The selected Proton version is not installed.");
                }
                runtimeFor(selected, libs);
                settings["proton"] = selected;
            }

            if (selectedPrefix.isNull() == false) {
                QString newPrefixStr = selectedPrefix.trimmed();
                if (!newPrefixStr.isEmpty()) {
                    if (newPrefixStr.startsWith("~/")) {
                        newPrefixStr = QDir::homePath() + newPrefixStr.mid(1);
                    }
                    QFileInfo prefixFi(newPrefixStr);
                    if (prefixFi.isFile()) {
                        throw std::runtime_error("The environment path must be a directory, not a file.");
                    }
                    if (prefixFi.fileName() == "pfx" && QDir(prefixFi.filePath() + "/drive_c").exists()) {
                        prefixFi = QFileInfo(prefixFi.dir().absolutePath());
                    }
                    QDir().mkpath(prefixFi.absoluteFilePath());
                    settings["prefix"] = prefixFi.canonicalFilePath().isEmpty() ? QDir::cleanPath(prefixFi.absoluteFilePath()) : prefixFi.canonicalFilePath();
                } else {
                    settings["prefix"] = defaultPrefix();
                }
            } else if (!settings.contains("prefix")) {
                settings["prefix"] = sharedPrefix(settings);
            }
            saveSettings(settings);
        }

        QJsonArray versionArr;
        for (const QString &v : versions) {
            QJsonObject vObj;
            vObj["name"] = QFileInfo(v).fileName();
            vObj["path"] = v;
            versionArr.append(vObj);
        }

        QJsonObject ret;
        ret["selected"] = settings.value("proton").toString();
        ret["versions"] = versionArr;
        ret["prefix"] = sharedPrefix(settings);
        ret["default_prefix"] = defaultPrefix();
        return ret;
    }

    QString prefix = sharedPrefix(settings);

    if (action == "toggle_shortcut") {
        if (shortcutId.isEmpty()) {
            throw std::runtime_error("Shortcut ID is required.");
        }
        return toggleShortcut(prefix, shortcutId, desktop, menu);
    }

    if (action == "running") {
        auto runningMap = getRunningApps(prefix);
        QJsonObject runObj;
        for (auto it = runningMap.constBegin(); it != runningMap.constEnd(); ++it) {
            runObj[it.key()] = !it.value().isEmpty();
        }
        QJsonObject ret;
        ret["running"] = runObj;
        return ret;
    }

    if (action == "kill") {
        if (key.isEmpty()) {
            throw std::runtime_error("Program key is required.");
        }
        return killProgram(prefix, key);
    }

    QString protonPath = settings.value("proton").toString();
    QString protonName = protonPath.isEmpty() ? "Not selected" : QFileInfo(protonPath).fileName();
    QString uninstallerExe = prefix + "/pfx/drive_c/windows/system32/uninstaller.exe";

    QJsonObject result;
    result["prefix"] = prefix;
    result["proton"] = protonName;
    result["programs"] = QJsonArray();
    result["ready"] = false;

    if (!QFile::exists(uninstallerExe)) {
        if (action != "list") {
            throw std::runtime_error("Windows environment does not exist yet.");
        }
        return result;
    }

    if (protonPath.isEmpty() || !QFile::exists(protonPath + "/proton")) {
        throw std::runtime_error("Select an installed Proton version in Settings first.");
    }

    QStringList roots = steamRoots();
    QStringList libs = steamLibraries(roots);

    auto queryPrograms = [&]() -> QList<QPair<QString, QString>> {
        QString captured;
        launch(uninstallerExe, protonPath, roots, libs, {"--list"}, prefix, true, "runinprefix", &captured);
        return parsePrograms(captured);
    };

    auto programList = queryPrograms();

    if (action == "uninstall") {
        QList<QPair<QString, QString>> matches;
        for (const auto &p : programList) {
            if (p.first.toLower() == key.toLower()) {
                matches.append(p);
            }
        }
        if (matches.size() != 1) {
            throw std::runtime_error("The app no longer exists, or its ID is ambiguous. Refresh the library.");
        }
        launch(uninstallerExe, protonPath, roots, libs, {"--remove", matches[0].first}, prefix, false, "runinprefix");
        programList = queryPrograms();
        bool stillExists = false;
        for (const auto &p : programList) {
            if (p.first.toLower() == key.toLower()) {
                stillExists = true;
                break;
            }
        }
        result["removed"] = !stillExists;
    }

    auto icons = programIcons(prefix);
    auto meta = getProgramRegistryMeta(prefix);
    QJsonArray basePrograms;
    for (const auto &p : programList) {
        QJsonObject pObj;
        pObj["key"] = p.first;
        QString displayName = p.second;
        if (meta.contains(p.first.toLower()) && !meta[p.first.toLower()].name.isEmpty()) {
            displayName = meta[p.first.toLower()].name;
        }
        pObj["name"] = displayName;
        QString icon = icons.value(displayName.toLower(), "");
        if (icon.isEmpty()) icon = icons.value(p.second.toLower(), "");
        pObj["icon"] = icon;
        basePrograms.append(pObj);
    }

    QJsonArray groupedPrograms = groupShortcutsByProgram(prefix, basePrograms);
    auto runningMap = getRunningApps(prefix, groupedPrograms);

    QJsonArray finalPrograms;
    for (const auto &val : groupedPrograms) {
        QJsonObject prog = val.toObject();
        QString k = prog.value("key").toString();
        QList<qint64> pids = runningMap.value(k);

        QJsonArray pidsArr;
        for (qint64 pid : pids) pidsArr.append(pid);

        prog["pids"] = pidsArr;
        prog["running"] = !pids.isEmpty();
        finalPrograms.append(prog);
    }

    result["ready"] = true;
    result["programs"] = finalPrograms;
    return result;
}

} // namespace WinBridge
